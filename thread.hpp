#ifndef _THREAD_H
#define _THREAD_H
#include <pthread.h>

class Thread
{
   public:
      Thread();
      ~Thread();
      int Start(void * arg);
   protected:
      void Run(void * arg);
      static void *EntryPoint(void*);
      virtual void Setup();
      virtual void Execute(void*);
      void * Arg() const {return Arg_;}
      void Arg(void* a){Arg_ = a;}
      pthread_t getThreadId() { return ThreadId_; }
   private:
      pthread_t ThreadId_;
      void * Arg_;

};
#endif
