#ifndef THREAD_H
#define THREAD_H

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define THREAD_RET DWORD WINAPI
#else
#include <pthread.h>
#define THREAD_RET void *
#endif

#include "Mutex.hpp"

THREAD_RET thread_start(void * obj);

class Thread
{
    public:
	Thread()
	{
	    _quit = false;
	    _quitMutex = new Mutex();
	}

	virtual ~Thread()
	{
	    delete _quitMutex;
	}

	virtual void run() = 0;

	void startThread()
	{
#ifdef WIN32
	    DWORD threadID;
	    _thread = CreateThread(NULL, 0, thread_start, (void*)this, 0, &threadID);
	    SetThreadPriority(_thread, THREAD_PRIORITY_HIGHEST);
#else
	    pthread_attr_t attr;

	    pthread_attr_init(&attr);
	    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	    pthread_create(&_thread,&attr,thread_start,(void*)this);
	    pthread_attr_destroy(&attr);
#endif
	}

	void quit()
	{
	    _quitMutex->lock();
	    _quit = true;
	    _quitMutex->unlock();
	}

	void join()
	{
	    quit();
#ifdef WIN32
	    WaitForSingleObject(_thread, INFINITE);
	    CloseHandle(_thread);
#else
	    void * status;
	    pthread_join(_thread,&status);
#endif
	}

    protected:
	bool _quit;
	Mutex * _quitMutex;

#ifdef WIN32
	HANDLE _thread;
#else
	pthread_t _thread;
#endif
};

#endif
