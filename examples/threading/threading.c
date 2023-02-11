#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

// TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
// hint: use a cast like the one below to obtain thread arguments from your parameter
struct thread_data* thread_func_args = (struct thread_data *) thread_param;
int thread_sleep = usleep(thread_func_args ->wait_to_obtain_ms * 1000);

//Sleep before release
if(thread_sleep== -1)
{
    perror("Error in Sleep");
    return thread_param;
}

//mutex lock
int mlock = pthread_mutex_lock(thread_func_args ->mutex);
if(mlock != 0)
{
    perror("Error in mutex lock");
    return thread_param;
}

//Sleep before release
int thread_slp = usleep(thread_func_args ->wait_to_release_ms * 1000);
if(thread_slp == -1)
{
    perror("Error in release Sleep");
    return thread_param;
}

//mutex unlock
int munlk = pthread_mutex_unlock(thread_func_args ->mutex);
if(munlk != 0)
{
    perror("Error in mutex unlock");
    return thread_param;
}
thread_func_args->thread_complete_success = true;
DEBUG_LOG("Success");
return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
/**
* TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
* using threadfunc() as entry point.
*
* return true if successful.
*
* See implementation details in threading.h file comment block
*/
struct thread_data *thread_enter = (struct thread_data *)malloc(sizeof(struct thread_data));

thread_enter->thread=*thread;
thread_enter->mutex=mutex;
thread_enter->wait_to_obtain_ms=wait_to_obtain_ms;
thread_enter->wait_to_release_ms=wait_to_release_ms;
int ret=pthread_create(thread,NULL,threadfunc,thread_enter);

if(thread_enter==NULL)
{
    ERROR_LOG("Memory Allocation Failed");
    return false;
}

if(ret!= 0)
{   
perror("pthread_create");
ERROR_LOG("thread creation failed");
return false;
}
if(ret==0)
{
return true;   
}

return false;
}

