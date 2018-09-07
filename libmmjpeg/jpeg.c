#include "jpeg.h"
#include "os.h"
#include <stdbool.h>
#include <string.h>

struct jpeg_alloc
{
	void* memory;
	const char* sourceFile;
	int sourceLine;
	bool isUsed;
};

static bool alreadyInitialized = false;
static struct jpeg_alloc jpegMemPool[200];
static unsigned char poolFullMarker;

static phtread_mutex_t jpegMemPoolLock;

void* jpeg_malloc(size_t size, const char* file, int line)
{
	void* memory = malloc(size);
	if (!alreadyInitialized)
	{
		memset(jpegMemPool, 0, sizeof(jpegMemPool));
		os_mutex_init(&jpegMemPoolLock);
		alreadyInitialized = true;
	}
	os_mutex_lock(&jpegMemPoolLock);
	if (memory && !poolFullMarker)
	{
		for (int i = 0; i < 200; ++i)
		{
			struct jpeg_alloc* alloc = &jpegMemPool[i];
			if (!alloc->isUsed)
			{
				alloc->isUsed = true;
				os_mutex_unlock(&jpegMemPoolLock);
				alloc->sourceFile = file;
				alloc->sourceLine = line;
				alloc->memory = memory;
				return memory;
			}
		}
		poolFullMarker = 1;
	}
	os_mutex_unlock(&jpegMemPoolLock);
	return memory;
}

void jpeg_free(void* memory)
{
	os_mutex_lock(&jpegMemPoolLock);
	if (!alreadyInitialized)
	{
		memset(jpegMemPool, 0, sizeof(jpegMemPool));
		alreadyInitialized = true;
	}
	free(memory);
	if (!poolFullMarker)
	{
		for (int i = 0; i < 200; ++i)
		{
			struct jpeg_alloc* alloc = &jpegMemPool[i];
			if (alloc->memory == memory && alloc->isUsed)
			{
				alloc->isUsed = false;
				break;
			}
		}
	}
	os_mutex_unlock(&jpegMemPoolLock);
}
