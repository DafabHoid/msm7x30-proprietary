/* rmt_storage userspace daemon
 * Copyright (C) 2018 DafabHoid
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>. */

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <linux/rmt_storage_client.h>
#include <sys/mman.h>
#include <pthread.h>
#define LOG_TAG "rmt_storage"
#include <log/log.h>

struct partReg
{
	unsigned char id;
	const char path[MAX_PATH_NAME];
	char buffer[MAX_PATH_NAME];
	int fd;
};

struct rmt_storage_client
{
	int fd;
	unsigned int signature;
	pthread_t threadID;
	pthread_mutex_t lock;
	pthread_cond_t cond;
	unsigned int eventID;
	struct rmt_storage_iovec_desc xfer_desc[RMT_STORAGE_MAX_IOVEC_XFR_CNT];
	unsigned int xferCount;
	char data[8];
	int lastErrorCode;
	int stopFlag;
	int userData;
	unsigned int storageID;
	struct rmt_shrd_mem_param* sharedMemParam;
};

#define SIGNATURE_MAGIC 0x12345678


/* Static data definitions */

static struct partReg partitionRegistry[] = {
{
	0x4A,
	"/boot/modem_fs1",
	{0},
	-1
},
{
	0x4B,
	"/boot/modem_fs2",
	{0},
	-1
},
{
	0x58,
	"/boot/modem_fsg",
	{0},
	-1
},
{
	0x59,
	"/q6_fs1_parti_id_0x59",
	{0},
	-1
},
{
	0x5A,
	"/q6_fs2_parti_id_0x5A",
	{0},
	-1
},
{
	0x5B,
	"/q6_fs1_parti_id_0x5B",
	{0},
	-1
},
{
	0x5C,
	"ssd",
	{0},
	-1
},
};
static const int partitionRegistryLength = sizeof(partitionRegistry) / sizeof(partitionRegistry[0]);

static const char MMCBLK0_PATH[] = "/dev/block/mmcblk0";
static unsigned char partitionBuffer[RAMFS_BLOCK_SIZE];
static int kernelDevFd;

static struct rmt_storage_client all_clients[MAX_NUM_CLIENTS];
static struct rmt_shrd_mem_param sharedMemParams[5];

/* End of static data */



static void* clientThread(void* arg);

static int readPartitionTable(int fd, unsigned char* buffer, long long num);

static int parseMMCPartitions();

static void destroyRMTSClient(struct rmt_storage_client* client);

static struct rmt_shrd_mem_param* findSharedMemParamBySID(unsigned int storageID);

int main()
{
	ALOGI("rmt_storage user app start.. ignoring cmd line parameters\n");
	
	// Set OOM value for this process
	{
		char oom_path[64];
		snprintf(oom_path, sizeof(oom_path), "/proc/%d/oom_adj", getpid());
		int oom_fd = open(oom_path, O_WRONLY);
		if (oom_fd < 0) {
			ALOGE("unable to write into %s", oom_path);
			return -1;
		}
		write(oom_fd, "-16", 3);
		close(oom_fd);
	}
	
	kernelDevFd = open("/dev/rmt_storage", O_RDWR);
	if (kernelDevFd < 0) {
		ALOGE("Unable to open /dev/rmt_storage\n");
		return -1;
	}
	
	ALOGI("rmt_storage open success\n");
	
	if (parseMMCPartitions() != 0) {
		ALOGE("Error parsing partitions\n");
		return -1;
	}
	
	int ret;
	ret = setgid(9999);
	if (ret < 0) {
		ALOGE("Error changing gid (%d)", ret);
		return -1;
	}
	ret = setuid(9999);
	if (ret < 0) {
		ALOGE("Error changing uid (%d)", ret);
		return -1;
	}
	
	while (1) {
		struct rmt_storage_event event;
		if (ioctl(kernelDevFd, RMT_STORAGE_WAIT_FOR_REQ, &event) < 0)
			break;
		switch (event.id) {
			
			case RMT_STORAGE_OPEN: {
				ALOGI("rmt_storage open event: handle=%d\n", event.handle);
				struct rmt_storage_client* client = &all_clients[event.handle - 1];
				client->storageID = event.sid;
				client->signature = SIGNATURE_MAGIC;
				pthread_mutex_init(&client->lock, NULL);
				pthread_cond_init(&client->cond, NULL);
				client->stopFlag = 0;
				int fd = -1;
				for (int i = 0; i < partitionRegistryLength; ++i) {
					struct partReg* part = &partitionRegistry[i];
					if (0 == strncmp(part->path, event.path, sizeof(part->path))) {
						fd = part->fd;
						break;
					}
				}
				client->fd = fd;
				if (fd < 0) {
					ALOGE("Unable to open %s\n", event.path);
				} else {
					ALOGI("Opened %s\n", event.path);
					client->sharedMemParam = NULL;
					if (0 == pthread_create(&client->threadID, NULL, clientThread, client)) {
						break;
					}
					ALOGE("Unable to create a pthread\n");
					close(client->fd);
				}
				destroyRMTSClient(client);
				break;
			}
			
			case RMT_STORAGE_READ:
			case RMT_STORAGE_WRITE: {
				ALOGI(event.id == 1 ? "rmt_storage write event\n" : "rmt_storage read event\n");
				struct rmt_storage_client* client = &all_clients[event.handle - 1];
				if (client->signature != SIGNATURE_MAGIC) {
					ALOGE("Invalid rmt_storage client\n");
					break;
				}
				struct rmt_shrd_mem_param* memParam = findSharedMemParamBySID(client->storageID);
				client->sharedMemParam = memParam;
				if (!memParam) {
					// Not yet retrieved from the kernel, so do it now
					memParam = findSharedMemParamBySID(0);
					if (!memParam)
						break;
					
					memParam->sid = client->storageID;
					if (ioctl(kernelDevFd, RMT_STORAGE_SHRD_MEM_PARAM, memParam) < 0) {
						ALOGE("rmt_storage shared memory ioctl failed\n");
						close(kernelDevFd);
						break;
					}
					void* result = mmap(NULL, memParam->size,
								PROT_READ|PROT_WRITE, MAP_SHARED, kernelDevFd, memParam->start);
					if (result == MAP_FAILED) {
						ALOGE("mmap failed for sid=0x%08x", client->storageID);
						close(kernelDevFd);
						break;
					}
					memParam->base = result;
					client->sharedMemParam = memParam;
				}
				if (!client->sharedMemParam) {
					ALOGE("Shared mem entry for 0x%x not found\n", client->storageID);
				}
				pthread_mutex_lock(&client->lock);
				client->eventID = event.id;
				memcpy(client->xfer_desc, event.xfer_desc, event.xfer_cnt * sizeof(event.xfer_desc[0]));
				client->xferCount = event.xfer_cnt;
				pthread_cond_signal(&client->cond);
				pthread_mutex_unlock(&client->lock);
				break;
			}
			
			case RMT_STORAGE_CLOSE: {
				ALOGI("rmt_storage close event\n");
				struct rmt_storage_client* client = &all_clients[event.handle - 1];
				if (client->signature != SIGNATURE_MAGIC) {
					ALOGE("Invalid rmt_storage client\n");
					break;
				}
				pthread_mutex_lock(&client->lock);
				client->stopFlag = 1;
				pthread_cond_signal(&client->cond);
				pthread_mutex_unlock(&client->lock);
				close(client->fd);
				break;
			}
			
			case RMT_STORAGE_SEND_USER_DATA: {
				ALOGI("rmt_storage send user data event\n");
				struct rmt_storage_client* client = &all_clients[event.handle - 1];
				if (client->signature != SIGNATURE_MAGIC) {
					ALOGE("Invalid rmt_storage client\n");
					break;
				}
				client->userData = event.usr_data;
				break;
			}
			
			default:
				ALOGI("unable to handle the event\n");
		}
		ALOGI("rmt_storage events processing done\n");
	}
	ALOGE("rmt_storage wait event ioctl failed errno=%d\n", errno);
	close(kernelDevFd);
	return 0;
}


static int readPartitionTable(int fd, unsigned char* buffer, long long num)
{
	if (!buffer)
		return -1;
	
	long long offset = (num & 0xFFFFFFFF) * RAMFS_BLOCK_SIZE | ((num >> 23) &~ 0xFFFFFFFF);
	long long result = lseek64(fd, offset, SEEK_SET);
	if (result != offset) {
		ALOGE("Error seeking 0x%llx bytes in partition. ret=0x%llx errno=%d\n", offset, result, errno);
		return -1;
	}
	
	int count = read(fd, buffer, RAMFS_BLOCK_SIZE);
	if (count != RAMFS_BLOCK_SIZE) {
		ALOGE("Error reading 0x%x bytes < 0x%x\n", count, RAMFS_BLOCK_SIZE);
		return -1;
	}
	
	if ((buffer[510] | (buffer[511] << 8)) != 0xAA55) {
		ALOGE("Invalid boot rec\n");
		return -1;
	}
	return 0;
}

static __inline int readOffsetWithSwappedHalfwords(unsigned char* base, int idx)
{
	return *(unsigned short*)&base[16 * (idx + 28) + 6]
        | (*(unsigned short*)&base[16 * (idx + 28) + 8] << 16);
}

static int parseMMCPartitions()
{
	int mmcblk0Fd = open(MMCBLK0_PATH, O_RDONLY);
	if (mmcblk0Fd < 0) {
		ALOGE("Unable to open %s\n", MMCBLK0_PATH);
		return 1;
	}
	if (readPartitionTable(mmcblk0Fd, partitionBuffer, 0LL) < 0) {
		close(mmcblk0Fd);
		return 1;
	}
	int idx = 0;
	if (partitionBuffer[450] != 5) {
		if (partitionBuffer[466] == 5) {
			idx = 1;
		} else if (partitionBuffer[482] == 5) {
			idx = 2;
		} else if (partitionBuffer[498] == 5) {
			idx = 3;
		} else {
			ALOGE("No EBR found\n");
			close(mmcblk0Fd);
			return 1;
		}
	}
	
	int numberOfPartitions = 0;
	int baseOffset = readOffsetWithSwappedHalfwords(partitionBuffer, idx);
	int offset = 0;
	idx = idx + 1;
	
	while (readPartitionTable(mmcblk0Fd, partitionBuffer, baseOffset + offset) >= 0) {
		int startIdx = idx;
		for (unsigned char* p = partitionBuffer; idx != startIdx + 2; p += 16) {
			char partID = p[450];
			if (!partID)
				goto end;
			if (partID == 5) {
				offset = readOffsetWithSwappedHalfwords(partitionBuffer, idx - startIdx);
			   break;
			}
			++idx;
			for (int i = 0; i < partitionRegistryLength; ++i) {
				struct partReg* part = &partitionRegistry[i];
				if (part->id == partID) {
					++numberOfPartitions;
					snprintf(part->buffer, sizeof(part->buffer), "%sp%d", MMCBLK0_PATH, idx);
					int fd = open(part->buffer, O_RDWR | O_TRUNC);
					part->fd = fd;
					if (fd < 0) {
						ALOGE("Unable to open %s\n", part->buffer);
					}
					break;
				}
			}
		}
	}
end:
	close(mmcblk0Fd);
	ALOGI("%d supported partitions found\n", numberOfPartitions);
	return 0;
}

void destroyRMTSClient(struct rmt_storage_client* client)
{
	client->signature = 0;
	pthread_mutex_destroy(&client->lock);
	pthread_cond_destroy(&client->cond);
	client->stopFlag = 0;
	client->fd = -1;
}

static struct rmt_shrd_mem_param* findSharedMemParamBySID(unsigned int storageID)
{
	for (unsigned int i = 0; i < sizeof(sharedMemParams)/sizeof(sharedMemParams[0]); ++i) {
		struct rmt_shrd_mem_param* mp = &sharedMemParams[i];
		if (mp->sid == storageID) {
			return mp;
		}
	}
	return NULL;
}

static void* clientThread(void* arg)
{
	struct rmt_storage_client* client = arg;
	ALOGI("rmt_storage client thread started\n");
	pthread_mutex_lock(&client->lock);
	
	ssize_t lastTransferredByteCount = 0;
	while (!client->stopFlag) {
		if (client->xferCount && !client->sharedMemParam) {
			ALOGE("No shared mem for sid=0x%08x\n", client->storageID);
			break;
		}
		for (unsigned int i = 0; i < client->xferCount; ++i) {
			struct rmt_storage_iovec_desc* xferDesc = &client->xfer_desc[i];
			
			lseek(client->fd, xferDesc->sector_addr * RAMFS_BLOCK_SIZE, SEEK_SET);
			
			struct rmt_shrd_mem_param* memParam = client->sharedMemParam;
			char* dataAddress = (char*)memParam->base - memParam->start + xferDesc->data_phy_addr;
			size_t length = xferDesc->num_sector * RAMFS_BLOCK_SIZE;
			
			if (client->eventID == 1) {
				lastTransferredByteCount = write(client->fd, dataAddress, length);
			} else {
				lastTransferredByteCount = read(client->fd, dataAddress, length);
			}
			
			if (lastTransferredByteCount < 0) {
				ALOGE("rmt_storage fop(%d) failed with error = %d \n", client->eventID, errno);
				break;
			}
			
			ALOGI("rmt_storage fop(%d): bytes transferred = %d\n", client->eventID, lastTransferredByteCount);
		}
		if (client->xferCount != 0) {
			int errorCode = lastTransferredByteCount < 0 ? lastTransferredByteCount : 0;
			client->lastErrorCode = errorCode;
			struct rmt_storage_send_sts sendStatus = {
				errorCode,
				client->userData,
				(client - all_clients) / sizeof(struct rmt_storage_client) + 1,
				client->eventID
			};
			if (ioctl(kernelDevFd, RMT_STORAGE_SEND_STATUS, &sendStatus) < 0) {
				ALOGE("rmt_storage send status ioctl failed\n");
			}
			client->xferCount = 0;
		}
		pthread_cond_wait(&client->cond, &client->lock);
		ALOGI("unblock rmt_storage client thread\n");
	}
	
	pthread_mutex_unlock(&client->lock);
	destroyRMTSClient(client);
	ALOGI("rmt_storage client thread exited\n");
	return NULL;
}






