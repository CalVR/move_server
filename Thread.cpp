#include "Thread.hpp"

THREAD_RET thread_start(void * obj)
{
    Thread * thread = (Thread*) obj;
    thread->run();
    return 0;
}

