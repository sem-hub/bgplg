#include "thread.hpp"
#include <pthread.h>

Thread::Thread() {}
Thread::~Thread() {}

int Thread::Start(void * arg)
{
    Arg(arg); // store user data
    int code = pthread_create(&ThreadId_, NULL, &Thread::EntryPoint, this);
    pthread_detach(ThreadId_);
    return code;
}

void Thread::Run(void * arg)
{
    Setup();
    Execute( arg );
}

/*static */
void * Thread::EntryPoint(void * pthis)
{
    Thread * pt = (Thread*)pthis;
    pt->Run( pt->Arg() );

    return NULL;
}

void Thread::Setup()
{
        // Do any setup here
}

void Thread::Execute(void* arg)
{
        // Your code goes here
}
