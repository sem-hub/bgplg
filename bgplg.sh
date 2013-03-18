#!/bin/sh

# PROVIDE: bgplg
# REQUIRE: NETWORKING
# KEYWORD: shutdown

. /etc/rc.subr

name=bgplg
rcvar=`set_rcvar`
command=/usr/local/sbin/bgplg
pidfile=/var/run/bgplg.pid

stop_postcmd=stop_postcmd

stop_postcmd()
{
	rm -f $pidfile
}

bgplg_enable=${bgplg_enable:-"NO"}

load_rc_config $name
run_rc_command "$1"
