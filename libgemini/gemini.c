/* MSM gemini (JPEG hardware encoder) userspace library
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

#define LOG_TAG "gemini"
#include "gemini.h"
#include <stdlib.h>
#include <log/log.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <media/msm_gemini.h> // Kernel header
#include <sys/mman.h>

#define LOGD(message, ...) \
	ALOGE("%s:%d] " message, __func__, __LINE__, ##__VA_ARGS__)

#define GEMINI_DEVICE "/dev/gemini0"

struct workerThread
{
	pthread_t tid;
	bool shouldStop;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	bool isReady;
};

struct gemini
{
	int deviceFd;
	eventThreadCallback_t eventThreadCallback;
	inputThreadCallback_t inputThreadCallback;
	outputThreadCallback_t outputThreadCallback;
	struct workerThread lib_event_thread;
	struct workerThread lib_input_thread;
	struct workerThread lib_output_thread;
	unsigned char cmd_type;
	int data1;
};


static void* gemini_lib_event_thread(void *arg);
static void* gemini_lib_input_thread(void *arg);
static void* gemini_lib_output_thread(void *arg);

static __inline void initWorkerThread(struct workerThread* thread)
{
	pthread_mutex_init(&thread->mutex, NULL);
	pthread_cond_init(&thread->cond, NULL);
	thread->isReady = false;
}

int gemini_lib_init(int **fdOut,
					eventThreadCallback_t eventThreadCallback,
					inputThreadCallback_t inputThreadCallback,
					outputThreadCallback_t outputThreadCallback)
{
	struct gemini* libgemini = malloc(sizeof(struct gemini));
	if ( !libgemini )
	{
		LOGD("no mem\n");
		return -1;
	}
	memset(libgemini, 0, sizeof(struct gemini));
	int fd = open(GEMINI_DEVICE, O_RDWR);
	ALOGE("open %s: fd = %d\n", GEMINI_DEVICE, fd);
	if ( fd < 0 )
	{
		ALOGE("Cannot open %s\n", GEMINI_DEVICE);
		free(libgemini);
		return -1;
	}
	libgemini->inputThreadCallback = inputThreadCallback;
	libgemini->outputThreadCallback = outputThreadCallback;
	libgemini->eventThreadCallback = eventThreadCallback;
	libgemini->deviceFd = fd;
	
	initWorkerThread(&libgemini->lib_event_thread);
	initWorkerThread(&libgemini->lib_input_thread);
	initWorkerThread(&libgemini->lib_output_thread);
	
	pthread_mutex_t* mutexToCleanup;
	if (eventThreadCallback)
	{
		pthread_mutex_lock(&libgemini->lib_event_thread.mutex);
		if (pthread_create(&libgemini->lib_event_thread.tid, NULL, gemini_lib_event_thread, libgemini) < 0)
		{
			ALOGE("%s event thread creation failed\n", __func__);
			mutexToCleanup = &libgemini->lib_event_thread.mutex;
			goto cleanup;
		}
		pthread_mutex_unlock(&libgemini->lib_event_thread.mutex);
	}
	if (inputThreadCallback)
	{
		pthread_mutex_lock(&libgemini->lib_input_thread.mutex);
		if (pthread_create(&libgemini->lib_input_thread.tid, NULL, gemini_lib_input_thread, libgemini) < 0)
		{
			ALOGE("%s input thread creation failed\n", __func__);
			mutexToCleanup = &libgemini->lib_input_thread.mutex;
			goto cleanup;
		}
		pthread_mutex_unlock(&libgemini->lib_input_thread.mutex);
	}
	if (outputThreadCallback)
	{
		pthread_mutex_lock(&libgemini->lib_output_thread.mutex);
		if (pthread_create(&libgemini->lib_output_thread.tid, NULL, gemini_lib_output_thread, libgemini) < 0)
		{
			ALOGE("%s output thread creation failed\n", __func__);
			mutexToCleanup = &libgemini->lib_output_thread.mutex;
			goto cleanup;
		}
		pthread_mutex_unlock(&libgemini->lib_output_thread.mutex);
	}
	ALOGE("gemini create all threads success\n");
	gemini_lib_wait_done(libgemini);
	ALOGE("gemini after starting all threads\n");
	*fdOut = &libgemini->deviceFd;
	return fd;
	
cleanup:
	pthread_mutex_unlock(mutexToCleanup);
	free(libgemini);
	return -1;
}

int gemini_lib_wait_done(struct gemini *lib)
{
	LOGD("gemini_lib_wait_thread_ready; event_handler %lu\n",
		  lib->lib_event_thread.tid);
	if ( lib->eventThreadCallback )
		gemini_lib_wait_thread_ready(lib, &lib->lib_event_thread.tid);
	
	LOGD("gemini_lib_wait_thread_ready: input_handler %lu\n",
		  lib->lib_input_thread.tid);
	if ( lib->inputThreadCallback )
		gemini_lib_wait_thread_ready(lib, &lib->lib_input_thread.tid);
	
	LOGD("gemini_lib_wait_thread_ready: output_handler\n");
	if ( lib->outputThreadCallback )
		gemini_lib_wait_thread_ready(lib, &lib->lib_output_thread.tid);
	
	LOGD("gemini_lib_wait_done\n");
	return 0;
}

void gemini_lib_wait_thread_ready(struct gemini *lib, pthread_t *tid)
{
	pthread_mutex_t* mutex;
	pthread_t threadId = *tid;
	LOGD("thread_id %lu\n", threadId);
	if ( threadId == lib->lib_event_thread.tid )
	{
		mutex = &lib->lib_event_thread.mutex;
		pthread_mutex_lock(mutex);
		LOGD("event thread ready %d\n",
			lib->lib_event_thread.isReady);
		if ( !lib->lib_event_thread.isReady )
			pthread_cond_wait(&lib->lib_event_thread.cond, mutex);
		lib->lib_event_thread.isReady = 0;
	}
	else if ( threadId == lib->lib_input_thread.tid )
	{
		mutex = &lib->lib_input_thread.mutex;
		pthread_mutex_lock(&lib->lib_input_thread.mutex);
		LOGD("ready %d\n",
			lib->lib_input_thread.isReady);
		if ( !lib->lib_input_thread.isReady )
			pthread_cond_wait(&lib->lib_input_thread.cond, &lib->lib_input_thread.mutex);
		lib->lib_input_thread.isReady = 0;
	}
	else if ( threadId == lib->lib_output_thread.tid )
	{
		mutex = &lib->lib_output_thread.mutex;
		pthread_mutex_lock(&lib->lib_output_thread.mutex);
		LOGD("ready %d\n",
			lib->lib_output_thread.isReady);
		if ( !lib->lib_output_thread.isReady )
			pthread_cond_wait(&lib->lib_output_thread.cond, &lib->lib_output_thread.mutex);
		lib->lib_output_thread.isReady = 0;
	}
	else
	{
		return LOGD("thread_id %lu done\n", threadId);
	}
	pthread_mutex_unlock(mutex);
	LOGD("thread_id %lu done\n", threadId);
}

static __inline void destroyWorkerThread(struct workerThread* thread)
{
	pthread_mutex_destroy(&thread->mutex);
	pthread_cond_destroy(&thread->cond);
}

void gemini_lib_release(struct gemini *lib)
{
	lib->lib_event_thread.shouldStop = 1;
	lib->lib_input_thread.shouldStop = 1;
	lib->lib_output_thread.shouldStop = 1;
	if ( lib->eventThreadCallback )
	{
		ioctl(lib->deviceFd, MSM_GMN_IOCTL_EVT_GET_UNBLOCK);
		LOGD("pthread_join: event_thread\n");
		if ( pthread_join(lib->lib_event_thread.tid, 0) )
			LOGD("failed\n");
	}
	if ( lib->inputThreadCallback )
	{
		ioctl(lib->deviceFd, MSM_GMN_IOCTL_INPUT_GET_UNBLOCK);
		LOGD("pthread_join: input_thread\n");
		if ( pthread_join(lib->lib_input_thread.tid, 0) )
			LOGD("failed\n");
	}
	if ( lib->outputThreadCallback )
	{
		ioctl(lib->deviceFd, MSM_GMN_IOCTL_OUTPUT_GET_UNBLOCK);
		LOGD("pthread_join: output_thread\n");
		if ( pthread_join(lib->lib_output_thread.tid, 0) )
			LOGD("failed\n");
	}
	close(lib->deviceFd);
	destroyWorkerThread(&lib->lib_event_thread);
	destroyWorkerThread(&lib->lib_input_thread);
	destroyWorkerThread(&lib->lib_output_thread);
	LOGD("closed\n");
}

int gemini_lib_stop(struct gemini *lib, int dontUnblock)
{
	int ret = 0;
	struct msm_gemini_hw_cmds* hw_stop = gemini_lib_hw_stop(&lib->cmd_type, dontUnblock);
	if (hw_stop)
	{
		int deviceFd = lib->deviceFd;
		LOGD("ioctl MSM_GMN_IOCTL_STOP\n");
		ret = ioctl(deviceFd, MSM_GMN_IOCTL_STOP, hw_stop);
		ALOGE("ioctl %s: rc = %d\n", GEMINI_DEVICE, ret);
		if (!dontUnblock)
		{
			ioctl(deviceFd, MSM_GMN_IOCTL_EVT_GET_UNBLOCK);
			ioctl(deviceFd, MSM_GMN_IOCTL_INPUT_GET_UNBLOCK);
			ioctl(deviceFd, MSM_GMN_IOCTL_OUTPUT_GET_UNBLOCK);
		}
		free(hw_stop);
	}
	return ret;
}

static void* gemini_lib_event_thread(void *arg)
{
	struct gemini* lib = arg;
	struct msm_gemini_ctrl_cmd gemin_ctrl_cmd;
	struct workerThread* thread = &lib->lib_event_thread;
	
	LOGD("Enter threadid %lu\n",
		thread->tid);
	gemini_lib_send_thread_ready(lib, thread);

	do
	{
		int ret = ioctl(lib->deviceFd, MSM_GMN_IOCTL_EVT_GET, &gemin_ctrl_cmd);
		LOGD("MSM_GMN_IOCTL_EVT_GET rc = %d\n", ret);
		if ( ret )
		{
			if ( !thread->shouldStop )
				LOGD("fail\n");
		}
		else
		{
			lib->eventThreadCallback(lib, &gemin_ctrl_cmd);
		}
		gemini_lib_send_thread_ready(lib, thread);
	}
	while ( !thread->shouldStop );
	
	LOGD("Exit\n");
	return NULL;
}

static void* gemini_lib_input_thread(void *arg)
{
	struct gemini *lib = arg;
	struct msm_gemini_buf gemini_buf;
	struct workerThread* thread = &lib->lib_input_thread;
	
	LOGD("Enter threadid %lu\n", thread->tid);
	
	gemini_lib_send_thread_ready(lib, thread);
	do
	{
		int ret = ioctl(lib->deviceFd, MSM_GMN_IOCTL_INPUT_GET, &gemini_buf);
		LOGD("MSM_GMN_IOCTL_INPUT_GET rc = %d\n", ret);
		if ( ret )
		{
			if ( !thread->shouldStop )
				LOGD("fail\n");
		}
		else
		{
			lib->inputThreadCallback(lib, &gemini_buf);
		}
		gemini_lib_send_thread_ready(lib, thread);
	}
	while ( !thread->shouldStop );
	
	LOGD("Exit\n");
	return NULL;
}

static void* gemini_lib_output_thread(void *arg)
{
	struct gemini *lib = arg;
	struct msm_gemini_buf gemini_buf;
	struct workerThread* thread = &lib->lib_output_thread;
	
	LOGD("Enter threadid %lu\n", thread->tid);
	
	gemini_lib_send_thread_ready(lib, thread);
	do
	{
		int ret = ioctl(lib->deviceFd, MSM_GMN_IOCTL_OUTPUT_GET, &gemini_buf);
		LOGD("MSM_GMN_IOCTL_OUTPUT_GET rc = %d\n", ret);
		if ( ret )
		{
			if ( !thread->shouldStop )
				LOGD("fail\n");
		}
		else
		{
			lib->outputThreadCallback(lib, &gemini_buf);
		}
		gemini_lib_send_thread_ready(lib, thread);
	}
	while ( !thread->shouldStop );
	
	LOGD("Exit\n");
	return NULL;
}

int gemini_lib_input_buf_enq(struct gemini *lib, struct msm_gemini_buf *buf)
{
	struct msm_gemini_buf geminibuf;
	
	geminibuf.type = buf->type;
	geminibuf.y_off = buf->y_off;
	geminibuf.y_len = buf->y_len;
	geminibuf.cbcr_len = buf->cbcr_len;
	geminibuf.num_of_mcu_rows = buf->num_of_mcu_rows;
	geminibuf.fd = buf->fd;
	geminibuf.vaddr = buf->vaddr;
	geminibuf.framedone_len = buf->framedone_len;
	geminibuf.cbcr_off = buf->cbcr_off;
	geminibuf.cbcr_len = buf->cbcr_len;
	geminibuf.num_of_mcu_rows = buf->num_of_mcu_rows;
	geminibuf.framedone_len = buf->framedone_len;
	geminibuf.cbcr_off = buf->cbcr_off;
	
	int ret = ioctl(lib->deviceFd, MSM_GMN_IOCTL_INPUT_BUF_ENQUEUE, &geminibuf);
	LOGD("inputbuf: 0x%p enqueue %d, result %d\n",
		buf->vaddr, buf->y_len, ret);
	return ret;
}

int gemini_lib_output_buf_enq(struct gemini *lib, struct msm_gemini_buf *buf)
{
	struct msm_gemini_buf geminibuf;
	
	geminibuf.type = buf->type;
	geminibuf.y_off = buf->y_off;
	geminibuf.y_len = buf->y_len;
	geminibuf.cbcr_len = buf->cbcr_len;
	geminibuf.num_of_mcu_rows = buf->num_of_mcu_rows;
	geminibuf.fd = buf->fd;
	geminibuf.vaddr = buf->vaddr;
	geminibuf.framedone_len = buf->framedone_len;
	geminibuf.cbcr_off = buf->cbcr_off;
	geminibuf.cbcr_len = buf->cbcr_len;
	geminibuf.num_of_mcu_rows = buf->num_of_mcu_rows;
	geminibuf.framedone_len = buf->framedone_len;
	geminibuf.cbcr_off = buf->cbcr_off;
	
	int ret = ioctl(lib->deviceFd, MSM_GMN_IOCTL_OUTPUT_BUF_ENQUEUE, &geminibuf);
	LOGD("outputbuf: 0x%p enqueue %d, result %d\n",
		buf->vaddr, buf->y_len, ret);
	return ret;
}

int gemini_lib_encode(struct gemini* lib)
{
	struct msm_gemini_hw_cmds* hw_start = gemini_lib_hw_start(&lib->cmd_type);
	if (!hw_start)
		return -1;
	
	int ret = ioctl(lib->deviceFd, MSM_GMN_IOCTL_START, hw_start);
	ALOGE("ioctl %s: rc = %d\n", GEMINI_DEVICE, ret);
	return ret;
}

void gemini_lib_send_thread_ready(struct gemini *lib, struct workerThread *thread)
{
	pthread_t threadId = thread->tid;
	
	LOGD("thread_id %lu\n", threadId);
	
	if ( threadId == lib->lib_event_thread.tid )
	{
		pthread_mutex_lock(&lib->lib_event_thread.mutex);
		lib->lib_event_thread.isReady = 1;
		pthread_cond_signal(&lib->lib_event_thread.cond);
		pthread_mutex_unlock(&lib->lib_event_thread.mutex);
	}
	else if ( threadId == lib->lib_input_thread.tid )
	{
		pthread_mutex_lock(&lib->lib_input_thread.mutex);
		lib->lib_input_thread.isReady = 1;
		pthread_cond_signal(&lib->lib_input_thread.cond);
		pthread_mutex_unlock(&lib->lib_input_thread.mutex);
	}
	else if ( threadId == lib->lib_output_thread.tid )
	{
		pthread_mutex_lock(&lib->lib_output_thread.mutex);
		lib->lib_output_thread.isReady = 1;
		pthread_cond_signal(&lib->lib_output_thread.cond);
		pthread_mutex_unlock(&lib->lib_output_thread.mutex);
	}
	
	LOGD("thread_id %lu done\n", thread->tid);
}

#define PMEM_DEVICE "/dev/pmem_adsp"

void* do_mmap(size_t allocSize, int *pmemFd)
{	
	int fd = open(PMEM_DEVICE, O_RDWR | O_DSYNC);
	LOGD("Open device %s!\n", PMEM_DEVICE);
	if (fd < 0)
	{
		LOGD("Open device %s failed!\n", PMEM_DEVICE);
		return NULL;
	}
	
	size_t size = (allocSize + 4095) & 0xFFFFF000;
	void* memory = mmap(NULL, size, 3, 1, fd, 0);
	if (memory == (void *)-1)
	{
		memory = NULL;
		LOGD("failed: %s (%d)\n", strerror(errno), errno);
	}
	LOGD("pmem_fd %d addr %p size %u\n", fd, memory, size);
	*pmemFd = fd;
	return memory;
}

int do_munmap(int pmemFd, void* memory, size_t allocSize)
{	
	int ret = 0;
	size_t size = (allocSize + 4095) & 0xFFFFF000;
	if (memory)
	{
		ret = munmap(memory, size);
	}
	close(pmemFd);
	LOGD("pmem_fd %d addr %p size %u rc %d\n", pmemFd, memory, size, ret);
	return ret;
}

static int createQuantisizerMatrix(uint8_t *matrixDest, const uint8_t *matrixBase, unsigned int quality)
{
	if (quality == 50) // Just copy over
	{
		for (int i = 0; i != 64; ++i)
		{
			matrixDest[i] = matrixBase[i];
		}
		return 0;
	}
	
	double frac;
	if (quality == 0)
	{
		quality = 1;
		frac = quality / 50.0;
	}
	else
	{
		if (quality >= 98)
			quality = 98;
		if (quality > 50)
		{
			frac = 50.0 / (100 - quality);
		} else {
			frac = quality / 50.0;
		}
	}
	for (int j = 0; j < 64; ++j)
	{
		matrixDest[j] = (uint8_t)(matrixBase[j] / frac + 0.5);
	}
	return 0;
}

static const unsigned char quantisizer1[8*8] = {
	16, 11, 10, 16,  24,  40,  51,  61,
	12, 12, 14, 19,  26,  58,  60,  55,
	14, 13, 16, 24,  40,  57,  69,  56,
	14, 17, 22, 29,  51,  87,  80,  62,
	18, 22, 37, 56,  68, 109, 103,  77,
	24, 35, 55, 64,  81, 104, 113,  92,
	49, 64, 78, 87, 103, 121, 120, 101,
	72, 92, 95, 98, 112, 100, 103,  99,
};
static const unsigned char quantisizer2[8*8] = {
	17, 18, 24, 47, 99, 99, 99, 99,
	18, 21, 26, 66, 99, 99, 99, 99,
	24, 26, 56, 99, 99, 99, 99, 99,
	47, 66, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99,
};

int gemini_app_calc_param(struct gemini_app_param *appParam, unsigned int param1, unsigned int jpegQuality, int param3, unsigned int param4, int param5, int param6)
{
	appParam->value0 = (unsigned int)(param3 - 1 + (1 << (param5 + 2))) >> (param5 + 2);
	appParam->flag = 0;
	appParam->valuei0 = 0;
	appParam->valuei1 = 0;
	appParam->valuei2 = 0;
	appParam->valuei3 = 0;
	
	createQuantisizerMatrix(appParam->quantMatrix1, quantisizer1, jpegQuality);
	if (createQuantisizerMatrix(appParam->quantMatrix2, quantisizer2, jpegQuality) != 0)
	{
		return 1;
	}
	
	unsigned int calc0 = 12 - param5 - param6;
	unsigned int calc1 = (param4 - 1 + (1 << (param6 + 2))) >> (param6 + 2);
	unsigned int calc2 = (calc1 + 15) >> 4;
	appParam->valuei5 = calc2 - 1;
	const double PI = 3.14159365;
	unsigned int calc3 = (unsigned int) ((param1 << 15) / ((param3 * param4) * PI));
	unsigned int product = calc3 * calc1;
	for (int i = 0; i <= 14; ++i)
	{
		if (calc1 <= calc2 || product == 0)
		{
			break;
		}
		signed int calc4 = (2 * calc3) >> calc0;
		signed int calc5;
		if (calc4 >= 0)
		{
			calc5 = calc4 < 255 ? calc4 : -1;
		}
		else
		{
			calc5 = 0;
		}
		calc1 -= calc2;
		unsigned int calc6 = calc2 * (calc5 << calc0);
		if (product > calc6)
			product -= calc6;
		else
			product = 0;
		calc3 = product / calc1;
	}
	return 0;
}

int gemini_lib_hw_config(struct gemini *lib,
						const struct gemini_input_cfg* inputCfg,
						const uint8_t* hw_we_cfg_params,
						const struct gemini_hw_cfg *pHwCfg,
						const struct gemini_op_cfg *pOpCfg)
{
	uint16_t huffmanValues4[512];
	uint16_t huffmanValues3[512];
	uint16_t huffmanValues2[24];
	uint16_t huffmanValues1[24];
	int deviceFd = lib->deviceFd;
	int ret;
	
	// Reset device
	struct msm_gemini_ctrl_cmd gemini_reset_ctrl_cmd = {pOpCfg->op_mode};
	ret = ioctl(deviceFd, MSM_GMN_IOCTL_RESET, &gemini_reset_ctrl_cmd);
	ALOGE("ioctl MSM_GMN_IOCTL_RESET: rc = %d\n", ret);
	if (ret != 0)
		goto fail;
	
	// Get HW version
	struct msm_gemini_hw_cmd getVersion; // [sp+880h] [bp-48h]
	gemini_lib_hw_get_version(&getVersion);
	ret = ioctl(deviceFd, MSM_GMN_IOCTL_GET_HW_VERSION, &getVersion);
	ALOGE("ioctl %s: rc = %d, version: %d\n", GEMINI_DEVICE, ret, getVersion.data);
	if (ret != 0)
		goto fail;
	
	// Configure FE
	struct msm_gemini_hw_cmds *hw_fe_cfg = gemini_lib_hw_fe_cfg(inputCfg);
	if (!hw_fe_cfg)
		goto fail;
	ret = ioctl(deviceFd, MSM_GMN_IOCTL_HW_CMDS, hw_fe_cfg);
	free(hw_fe_cfg);
	ALOGE("ioctl gemini_lib_hw_fe_cfg: rc = %d\n", ret);
	if (ret != 0)
		goto fail;

	// Configure output params
	struct gemini_output_cfg outputCfg = {
		.inputFormat = inputCfg->inputFormat,
		.param = inputCfg->params[1],
		.frame_width_mcus = inputCfg->frame_width_mcus,
		.frame_height_mcus = inputCfg->frame_height_mcus,
	};
	struct msm_gemini_hw_cmds *hw_op_cfg = gemini_lib_hw_op_cfg(pOpCfg, &outputCfg);
	if (!hw_op_cfg)
		goto fail;
	ret = ioctl(deviceFd, MSM_GMN_IOCTL_HW_CMDS, hw_op_cfg);
	free(hw_op_cfg);
	ALOGE("ioctl gemini_lib_hw_op_cfg: rc = %d\n", ret);
	if (ret != 0)
		goto fail;
	
	// Configure WE
	struct msm_gemini_hw_cmds *hw_we_cfg = gemini_lib_hw_we_cfg(hw_we_cfg_params);
	if (!hw_we_cfg)
		goto fail;
	ret = ioctl(deviceFd, MSM_GMN_IOCTL_HW_CMDS, hw_we_cfg);
	free(hw_we_cfg);
	ALOGE("ioctl gemini_lib_hw_we_cfg: rc = %d\n", ret);
	if (ret != 0)
		goto fail;
	
	// Configure pipeline
	struct gemini_pipeline_cfg pipelineCfg = {
		.inputFormat = inputCfg->inputFormat,
		.op_mode = pOpCfg->op_mode,
		.data = {0, 0, 0, 0, 0},
	};
	struct msm_gemini_hw_cmds *hw_pipeline_cfg = gemini_lib_hw_pipeline_cfg(&pipelineCfg);
	if (!hw_pipeline_cfg)
		goto fail;
	ret = ioctl(deviceFd, MSM_GMN_IOCTL_HW_CMDS, hw_pipeline_cfg);
	free(hw_pipeline_cfg);
	ALOGE("ioctl gemini_lib_hw_pipeline_cfg: rc = %d\n", ret);
	if (ret != 0)
		goto fail;
	
	// Set restart marker
	struct msm_gemini_hw_cmds *hw_restart_marker_set = gemini_lib_hw_restart_marker_set(pHwCfg->restartMarker);
	if (!hw_restart_marker_set)
		goto fail;
	ret = ioctl(deviceFd, MSM_GMN_IOCTL_HW_CMDS, hw_restart_marker_set);
	free(hw_restart_marker_set);
	ALOGE("ioctl restart marker: rc = %d\n", ret);
	if (ret != 0)
		goto fail;
	
	if (pHwCfg->huffmanTablesAllocated)
	{
		// Set huffman tables
		gemini_lib_hw_create_huffman_tables(pHwCfg, huffmanValues1, huffmanValues2, huffmanValues3, huffmanValues4);
		struct msm_gemini_hw_cmds *hw_set_huffman_tables = gemini_lib_hw_set_huffman_tables(
				huffmanValues1,
				huffmanValues2,
				huffmanValues3,
				huffmanValues4);
		if (!hw_set_huffman_tables)
		{
			goto fail;
		}
		ret = ioctl(deviceFd, MSM_GMN_IOCTL_HW_CMDS, hw_set_huffman_tables);
		free(hw_set_huffman_tables);
		ALOGE("ioctl huffman: rc = %d\n", ret);
		if (ret != 0)
		{
			goto fail;
		}
	}
	
	const uint8_t* quantTable1 = pHwCfg->quantTable[0];
	const uint8_t* quantTable2 = pHwCfg->quantTable[1];
	if (quantTable1 && quantTable2)
	{
		// Set quantization tables and test to read it
		struct msm_gemini_hw_cmds *hw_set_quant_tables = gemini_lib_hw_set_quant_tables(quantTable1, quantTable2);
		if (!hw_set_quant_tables)
			goto fail;
		ret = ioctl(deviceFd, MSM_GMN_IOCTL_HW_CMDS, hw_set_quant_tables);
		free(hw_set_quant_tables);
		ALOGE("ioctl gemini_lib_hw_set_quant_tables: rc = %d\n", ret);
		if (ret != 0)
			goto fail;
		struct msm_gemini_hw_cmds *hw_read_quant_tables = gemini_lib_hw_read_quant_tables();
		if (!hw_read_quant_tables)
			goto fail;
		ret = ioctl(deviceFd, MSM_GMN_IOCTL_HW_CMDS, hw_read_quant_tables);
		free(hw_read_quant_tables);
	}
	
	if (pHwCfg->setFilesizeCtrl)
	{
		// Configure filesize control
		struct msm_gemini_hw_cmds *hw_set_filesize_ctrl = gemini_lib_hw_set_filesize_ctrl(&pHwCfg->filesizeCtrlCfg);
		if (!hw_set_filesize_ctrl)
			goto fail;
		ret = ioctl(deviceFd, MSM_GMN_IOCTL_HW_CMDS, hw_set_filesize_ctrl);
		free(hw_set_filesize_ctrl);
		ALOGE("ioctl gemini_lib_hw_set_filesize_ctrl: rc = %d\n", ret);
		if (ret != 0)
			goto fail;
	}
	
	memcpy(&lib->cmd_type, &pOpCfg->op_mode, 2*sizeof(int));
	LOGD("success\n");
	return ret;
	
fail:
	LOGD("fail\n");
	return ret;
}
