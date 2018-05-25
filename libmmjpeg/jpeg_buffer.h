#ifndef JPEG_BUFFER_H
#define JPEG_BUFFER_H

#include <pthread.h>

struct jpeg_buffer
{
	void* address;
	void* phyAddress;
	int data2;
	size_t capacity;
	size_t usedSize;
	int pmemFd;
	char isValid;
	char allocated;
	char isEmpty;
	char isBusy;
	pthread_mutex_t lock;
	pthread_cond_t cond;
	size_t offset;
};

int jpeg_buffer_get_pmem_fd(struct jpeg_buffer*);
int jpeg_buffer_get_addr(struct jpeg_buffer*, void** addrOut);
int jpeg_buffer_get_max_size(struct jpeg_buffer*, size_t** sizeOut);
int jpeg_buffer_get_actual_size(struct jpeg_buffer*, size_t** sizeOut);
int jpeg_buffer_set_actual_size(struct jpeg_buffer*, size_t* size);
int jpeg_buffer_set_start_offset(struct jpeg_buffer*, size_t offset);

int jpeg_buffer_attach_existing(struct jpeg_buffer* buf1, struct jpeg_buffer* buf2, size_t offset);

void jpeg_buffer_mark_busy(struct jpeg_buffer*);
void jpeg_buffer_mark_filled(struct jpeg_buffer*);
void jpeg_buffer_mark_empty(struct jpeg_buffer*);

void jpeg_buffer_wait_until_empty(struct jpeg_buffer*);
void jpeg_buffer_wait_until_filled(struct jpeg_buffer*);

int jpeg_buffer_init(struct jpeg_buffer** bufPtr)
void jpeg_buffer_destroy(struct jpeg_buffer** bufPtr);

int jpeg_buffer_allocate(struct jpeg_buffer*, size_t size, int usePmem);
int jpeg_buffer_reset(struct jpeg_buffer*);

int jpeg_buffer_use_external_buffer(struct jpeg_buffer*, void* address, size_t size, int pmemFd);

#endif
