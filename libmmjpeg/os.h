#include <pthread.h>

struct timespec;

__inline int os_cond_broadcast(pthread_cond_t* cond)
{
	return pthread_cond_broadcast(cond);
}

__inline int os_cond_wait(pthread_cond_t* cond, pthread_mutex_t* lock)
{
	return pthread_cond_wait(cond, lock);
}

__inline int os_cond_signal(pthread_cond_t* cond)
{
	return pthread_cond_signal(cond);
}

__inline int os_cond_init(pthread_cond_t* cond)
{
	return pthread_cond_init(cond, NULL);
}

__inline int os_cond_destroy(pthread_cond_t* cond)
{
	return pthread_cond_destroy(cond);
}

__inline int os_mutex_init(pthread_mutex_t* lock)
{
	return pthread_mutex_init(lock, NULL);
}

__inline int os_mutex_destroy(pthread_mutex_t* lock)
{
	return pthread_mutex_destroy(lock);
}

__inline int os_mutex_lock(pthread_mutex_t* lock)
{
	return pthread_mutex_lock(lock);
}

__inline int os_mutex_unlock(pthread_mutex_t* lock)
{
	return pthread_mutex_unlock(lock);
}

__inline pthread_t os_thread_self()
{
	return pthread_self();
}

__inline int os_thread_detach(pthread_t* thread)
{
	return pthread_detach(*thread);
}

__inline int os_thread_join(pthread_t* thread, void** result)
{
	return pthread_join(*thread, result);
}

__inline int os_thread_create(pthread_t* thread, void* (*func)(void*) void* arg)
{
	return pthread_create(thread, NULL, func, arg);
}

int os_cond_timedwait(pthread_cond_t* cond, pthread_mutex_t* lock, unsigned int millis);

int os_timer_start(struct timespec* t);

int os_timer_get_elapsed(struct timespec* ts, unsigned int* millis, int updateTime);



int os_pmem_fd_open(int* fd);
int os_pmem_fd_close(int* fd);

int os_pmem_allocate(int fd, size_t size, void** memory);
int os_pmem_free(int fd, size_t size, void* memory);

int os_pmem_get_phy_addr(int fd, void** address);










