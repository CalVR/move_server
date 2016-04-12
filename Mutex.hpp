#ifndef MUTEX_H
#define MUTEX_H

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#endif

class Mutex
{
    public:
	Mutex()
	{
	    _error = false;
#ifdef WIN32
	    _mutex = CreateMutex(NULL, FALSE, NULL);
	    if(!_mutex)
	    {
		_error = true;
	    }
#else
	    int ret = pthread_mutex_init(&_mutex,NULL);
	    if(!ret)
	    {
		_error = true;
	    }
#endif
	}

	~Mutex()
	{
	    if(!_error)
	    {
#ifdef WIN32
		CloseHandle(_mutex);
#else
		pthread_mutex_destroy(&_mutex);
#endif
	    }
	}

	bool error()
	{
	    return _error;
	}

	void lock()
	{
	    if(!_error)
	    {
#ifdef WIN32
		WaitForSingleObject(_mutex, INFINITE);
#else
		pthread_mutex_lock(&_mutex);
#endif
	    }
	}

	void unlock()
	{
	    if(!_error)
	    {
#ifdef WIN32
		ReleaseMutex(_mutex);
#else
		pthread_mutex_unlock(&_mutex);
#endif
	    }
	}

    protected:
	bool _error;
#ifdef WIN32
	HANDLE _mutex;
#else
	pthread_mutex_t _mutex;
#endif
};

#endif
