#include "jpeg_buffer.h"
#include "os.h"
#include "errors.h"

#define CHECK_VALID(buf) if (!buf || !buf->isValid) return INVALID_ARGUMENT
#define CHECK_HAS_ADDRESS(buf) CHECK_VALID(buf);\
	if (buf->address == NULL) return INVALID_ARGUMENT

int jpeg_buffer_get_pmem_fd(struct jpeg_buffer* buf)
{
	if (buf && buf->isValid && buf->address != NULL)
		return buf->pmemFd;
	else
		return -1;
}

int jpeg_buffer_get_addr(struct jpeg_buffer* buf, void** addrOut)
{
	CHECK_HAS_ADDRESS(buf);
	if (!addrOut)
		return INVALID_ARGUMENT;
	*addrOut = buf->address;
	return OK;
}

int jpeg_buffer_get_max_size(struct jpeg_buffer*, size_t** sizeOut)
{
	CHECK_HAS_ADDRESS(buf);
	if (!sizeOut)
		return INVALID_ARGUMENT;
	*sizeOut = buf->capacity;
	return OK;
}

int jpeg_buffer_get_actual_size(struct jpeg_buffer* buf, size_t** sizeOut)
{
	CHECK_HAS_ADDRESS(buf);
	if (!sizeOut)
		return INVALID_ARGUMENT;
	*sizeOut = buf->size;
	return OK;
}

int jpeg_buffer_set_actual_size(struct jpeg_buffer* buf, size_t* size)
{
	CHECK_VALID(buf);
	if (size > buf->capacity)
		return INVALID_ARGUMENT;
	buf->size = size;
	return OK;
}

int jpeg_buffer_set_start_offset(struct jpeg_buffer* buf, size_t offset)
{
	CHECK_VALID(buf);
	if (offset > buf->capacity)
		return INVALID_ARGUMENT;
	buf->offset = offset;
	return OK;
}


int jpeg_buffer_attach_existing(struct jpeg_buffer* buf, struct jpeg_buffer* buf2, size_t offset)
{
	CHECK_VALID(buf);
	CHECK_HAS_ADDRESS(buf2);
	if (buf->allocated || offset >= buf2->capacity)
		return INVALID_ARGUMENT;
	
	buf->allocated = 0;
	buf->phyAddress = (char*)buf2->phyAddress + offset;
	buf->address = (char*)buf2->address + offset;
	buf->capacity = buf2->capacity - offset;
	buf->pmemFd = buf2->pmemFd;
	buf->size = 0;
	return OK;
}

void jpeg_buffer_mark_busy(struct jpeg_buffer* buf)
{
	os_mutex_lock(&buf->lock);
	buf->isBusy = 1;
	os_mutex_unlock(&buf->lock);
}

void jpeg_buffer_mark_filled(struct jpeg_buffer* buf)
{
	os_mutex_lock(&buf->lock);
	buf->isEmpty = 0;
	buf->isBusy = 0;
	os_cond_signal(&buf->cond);
	os_mutex_unlock(&buf->lock);
}

void jpeg_buffer_mark_empty(struct jpeg_buffer* buf)
{
	os_mutex_lock(&buf->lock);
	buf->isEmpty = 1;
	buf->isBusy = 0;
	os_cond_signal(&buf->cond);
	os_mutex_unlock(&buf->lock);
}

void jpeg_buffer_wait_until_empty(struct jpeg_buffer* buf)
{
	os_mutex_lock(&buf->lock);
	while (!buf->isEmpty) {
		os_cond_wait(&buf->cond);
	}
	os_mutex_unlock(&buf->lock);
}

void jpeg_buffer_wait_until_filled(struct jpeg_buffer* buf)
{
	os_mutex_lock(&buf->lock);
	while (buf->isEmpty) {
		os_cond_wait(&buf->cond);
	}
	os_mutex_unlock(&buf->lock);
}


void jpeg_buffer_destroy(struct jpeg_buffer** bufPtr)
{
	if (bufPtr) {
		struct jpeg_buffer* buf = *bufPtr;
		if (buf) {
			jpeg_buffer_reset(buf);
			os_mutex_destroy(&buf->lock);
			os_cond_destroy(&buf->cond);
			jpeg_free(buf);
		}
	}
}

int jpeg_buffer_init(struct jpeg_buffer** bufPtr)
{
	if (!bufPtr)
		return BAD_HANDLE;
	struct jpeg_buffer* buf = jpeg_malloc(sizeof(struct jpeg_buffer), __FILE__, __LINE__);
	*bufPtr = buf;
	if (!buf)
		return OUT_OF_MEMORY;
	memset(buf, 0, sizeof(buf));
	buf->pmemFd = -1;
	buf->isValid = 1;
	buf->isEmpty = 1;
	os_mutex_init(&buf->lock);
	os_cond_init(&buf->cond);
	return OK;
}

int jpeg_buffer_reset(struct jpeg_buffer* buf)
{
	CHECK_VALID(buf);
	if (buf->allocated && buf->address != NULL) {
		if (buf->pmemFd == -1) {
			jpeg_free(buf->address);
		} else {
			os_pmem_free(buf->pmemFd, buf->capacity, buf->address);
			os_pmem_fd_close(&buf->pmemFd);
		}
	}
	buf->allocated = 0;
	buf->address = NULL;
	buf->phyAddress = NULL;
	buf->capacity = 0;
	buf->size = 0;
	return OK;
}

int jpeg_buffer_allocate(struct jpeg_buffer* buf, size_t size, int usePmem)
{
	CHECK_VALID(buf);
	if (buf->address != NULL || size == 0) {
		return INVALID_ARGUMENT;
	}
	
	if (usePmem) {
		int res = os_pmem_fd_open(&buf->pmemFd);
		if (res == 0) {
			buf->capacity = size;
			res = os_pmem_allocate(buf->pmemFd, size, &buf->address);
			if (res == 0) {
				res = os_pmem_get_phy_addr(buf->pmemFd, &buf->phyAddress);
			}
		}
		
		if (res != 0) {
			os_pmem_fd_close(&buf->pmemFd);
			buf->address = NULL;
			buf->capacity = 0;
			return res;
		}
	} else {
		buf->address = jpeg_malloc(size, __FILE__, __LINE__);
		if (!buf->address) {
			return OUT_OF_MEMORY;
		}
		buf->capacity = size;
	}
	buf->size = 0;
	buf->allocated = 1;
	return OK;
}

int jpeg_buffer_use_external_buffer(struct jpeg_buffer* buf, void* address, size_t size, int pmemFd)
{
	CHECK_VALID(buf);
	if (size == 0 || buf->allocated || address == 0)
		return INVALID_ARGUMENT;
	
	if (os_pmem_get_phy_addr(pmemFd, &buf->phyAddress) == 0) {
		buf->pmemFd = pmemFd;
	}
	buf->address = address;
	buf->capacity = size;
	return OK;
}
