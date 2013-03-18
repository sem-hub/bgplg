#include <iostream>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/param.h>
#include <pthread.h>
#include <libutil.h>
#include <list>
#include <cxxabi.h>
#include <log4cpp/Category.hh>
#include <log4cpp/RollingFileAppender.hh>
#include <log4cpp/PatternLayout.hh>
#include "global.hpp"
#include "socket.hpp"
#include "bgp_attributes.hpp"
#include "bgp_worker.hpp"
#include "http_worker.hpp"
#include "object_collection.hpp"
#include "rt.hpp"
#include "rwlock.hpp"
#include "ip_addr.hpp"
#include "config.hpp"

using namespace std;
using namespace log4cpp;

struct statistic statistic = { 0, 0, 0, 0, 0, 0 };
class object_collection objs;
class RadixTree rt;
list<pair<ip_addr, bgp_worker *> > peers;
pthread_rwlock_t peers_lock;
ip_addr bindip;
uint32_t ouras;
bool dont_write_sql = false;
MYSQL *mysql;
string mysqlhost, mysqluser, mysqlpass;

void
sighup_handler(int n)
{
    Appender::reopenAll();
}

// A replacement for the standard terminate_handler which prints
// more information about the terminating exception (if any) in log.
// WARNING!!! It's a hacked version of __gnu_cxx::__verbose_terminate_handler()
// WARNING!!! It may cause a compatibility problem with other gcc compillers.
void terminate_handler()
{
    log4cpp::Category &slog = log4cpp::Category::getInstance("bgplg");

    static bool terminating;
    if (terminating)
      {
	slog.log(log4cpp::Priority::ERROR, "terminate called recursively");
	abort ();
      }
    terminating = true;

    // Make sure there was an exception; terminate is also called for an
    // attempt to rethrow when there is no suitable exception.
    type_info *t = __cxxabiv1::__cxa_current_exception_type();
    if (t)
      {
	// Note that "name" is the mangled name.
	char const *name = t->name();
	{
	  int status = -1;
	  char *dem = 0;
	  
	  dem = __cxxabiv1::__cxa_demangle(name, 0, 0, &status);

	  slog.log(log4cpp::Priority::ERROR, "terminate called after throwing an instance of '");
	  if (status == 0)
	    slog.log(log4cpp::Priority::ERROR, dem);
	  else
	    slog.log(log4cpp::Priority::ERROR, name);

	  if (status == 0)
	    free(dem);
	}

	// If the exception is derived from std::exception, we can
	// give more information.
	try { __throw_exception_again; }
	catch (exception &exc)
	  {
	    char const *w = exc.what();
	    slog.log(log4cpp::Priority::ERROR, "  what():  ");
	    slog.log(log4cpp::Priority::ERROR, w);
	  }
	catch (...) { }
      }
    else
      slog.log(log4cpp::Priority::ERROR, "terminate called without an active exception\n");

    abort();
}

int
main(int argc, char *argv[], char* envp[])
{
    char	c;
    int		debug=0;
    pid_t	opid;
    struct pidfh	*pfh;
    bgp_worker	*bgp_session;
    http_worker	*http_session;
    struct servent	*se;
    Socket	*clsock;
    string	configFile("bgplg.conf");
    string	logfile, journalfile;
    uint16_t	http_port=8000;
    u_long	maxLogSize=1073741824L;

    while ((c = getopt(argc, argv, "dc:")) != -1) {
	switch (c) {
	    case 'd':
		++debug;
		break;
	    case 'c':
		configFile = optarg;
		break;
	    default:
		errx(1, "Unknown option: -%c", c);
	}
    }
    argc -= optind;
    argv += optind;
    
    /* Read a config file */
    Config config(configFile, envp);
    try {
	maxLogSize = config.pLong("maxLogSize");
    } catch(runtime_error) {
    	/* Ignore errors for optional parameters */
    }

    try {
	logfile = config.pString("logFile");
    } catch(runtime_error) {
	logfile = "/var/log/bgplg.log";
    }

    try {
	journalfile = config.pString("journalFile");
    } catch(runtime_error) {
	journalfile = "/var/log/bgplg-updates.log";
    }

    try {
	mysqlhost = config.pString("mysqlHost");
	mysqluser = config.pString("mysqlUser");
	mysqlpass = config.pString("mysqlPass");
    } catch(runtime_error) {
	dont_write_sql = true;
    }

    /* Create logging object */
    Appender* app = new RollingFileAppender("FileAppender",
	    /* fileName, maxFileSize, maxBackupIndex */
	    logfile, maxLogSize, 9);
    PatternLayout* layout = new PatternLayout();
    layout->setConversionPattern("%d %c %p [%t]: %m%n");
    app->setLayout(layout);

    Appender* app_journal = new RollingFileAppender("FileAppender",
	    journalfile, maxLogSize, 9);
    PatternLayout* layout_journal = new PatternLayout();
    layout_journal->setConversionPattern("%d %m%n");
    app_journal->setLayout(layout_journal);

    Category &slog = Category::getInstance("bgplg");
    slog.setAdditivity(false);
    slog.setAppender(app);
    /* default log level */
    if(debug)
	slog.setPriority(Priority::DEBUG);
    else
	slog.setPriority(Priority::INFO);

    Category &jlog = Category::getInstance("bgplg-journal");
    jlog.setAdditivity(false);
    jlog.setAppender(app_journal);
    jlog.setPriority(Priority::INFO);

    /* Change a default terminate handler to catch an exception message
     * and write it to log */
    std::set_terminate(::terminate_handler);

    slog.infoStream() << "Config file: " << configFile << eol;

    if(!dont_write_sql) {
	mysql = mysql_init(NULL);
	if(mysql == NULL) {
	    slog.log(log4cpp::Priority::ERROR, "mysql_init error %u: %s", mysql_errno(mysql), mysql_error(mysql));
	    dont_write_sql = true;
	} else {
	    if(mysql_real_connect(mysql, mysqlhost.c_str(), mysqluser.c_str(), mysqlpass.c_str(), NULL, 0, NULL, 0) == NULL) {
		slog.log(log4cpp::Priority::ERROR, "Can't connect to MySQL server (error %u: %s). Will not write a database.", mysql_errno(mysql), mysql_error(mysql));
		dont_write_sql = true;
	    }
	}
    }

    bindip.set_addr(config.pString("bindIP"));
    ouras = config.pInt("ourAS");
    try {
	http_port = config.pInt("httpPort");
    } catch(runtime_error) {
    	/* Ignore errors for optional parameters */
    }

    if ((pfh = pidfile_open("/var/run/bgplg.pid", 0644, &opid)) == NULL) {
	if (errno == EEXIST)
	    errx(1, "Already run with PID %d. Exiting.", opid);
	errx(1, "Can't create PID file");
    }

    if (!debug && daemon(0,0) == -1)
	errx(1, "Can't daemonize");

    pidfile_write(pfh);

    rwlock::init(peers_lock);

    /* Ignore broken pipe */
    signal(SIGPIPE, SIG_IGN);

    /* Reopen log files */
    signal(SIGHUP, sighup_handler);

    statistic.start_time = time(NULL);
    slog.log(log4cpp::Priority::INFO, "Runned");

    Socket bgp_socket;
    Socket http_socket;
    try {
	bgp_socket.create(SOCK_STREAM);
	http_socket.create(SOCK_STREAM);

	if((se=::getservbyname("bgp", "tcp")) == NULL)
	    throw "getservbyname";
	bgp_socket.listen(bindip.as_in_addr(), se->s_port);
	http_socket.listen(bindip.as_in_addr(), htons(http_port));

	while(1) {
	    if(bgp_socket.is_ready()) {
		clsock = bgp_socket.accept();
		bgp_session = new bgp_worker;
		bgp_session->Start(reinterpret_cast<void*>(clsock));
	    }
	    if(http_socket.is_ready()) {
		clsock = http_socket.accept();
		http_session = new http_worker;
		http_session->Start(reinterpret_cast<void*>(clsock));
	    }
	}
    } catch(...) {
	slog.log(log4cpp::Priority::ERROR, "An exception has happen");
    }

    if(!dont_write_sql)
	mysql_close(mysql);

    return 1;
}
