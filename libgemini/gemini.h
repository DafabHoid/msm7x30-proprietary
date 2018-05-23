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

#include <stdbool.h>
#include <stdint.h>

typedef long pthread_t;

struct gemini;
struct workerThread;
struct msm_gemini_hw_cmds;
struct msm_gemini_hw_cmd;
struct msm_gemini_buf;
struct msm_gemini_ctrl_cmd;

struct gemini_input_cfg
{
	unsigned int inputFormat;
	unsigned char params[3];
	unsigned int frame_height_mcus;
	unsigned int frame_width_mcus;
};

struct gemini_app_param
{
	unsigned short value0;
	bool flag;
	int valuei0;
	int valuei1;
	int valuei2;
	int valuei3;
	uint8_t *quantMatrix1;
	uint8_t *quantMatrix2;
	unsigned int valuei4;
	unsigned int valuei5;
};

struct gemini_pipeline_cfg
{
	uint8_t op_mode; // one of MSM_GEMINI_MODE_*
	uint8_t inputFormat;
	uint8_t data[5];
};

struct gemini_op_cfg
{
	unsigned char op_mode; // one of MSM_GEMINI_MODE_*
	uint32_t value;
};

struct gemini_output_cfg
{
	unsigned int inputFormat;
	uint32_t param;
	unsigned int frame_width_mcus;
	unsigned int frame_height_mcus;
};

struct gemini_filesize_ctrl_cfg
{
	unsigned int value;
	uint8_t data[16];
};

struct gemini_hw_cfg
{
	unsigned short restartMarker;
	bool huffmanTablesAllocated;
	uint8_t* huffmanTable[4];
	uint8_t* quantTable[2];
	bool setFilesizeCtrl;
	struct gemini_filesize_ctrl_cfg filesizeCtrlCfg;
};

typedef void (*eventThreadCallback_t)(struct gemini *, struct msm_gemini_ctrl_cmd *);
typedef void (*inputThreadCallback_t)(struct gemini *, struct msm_gemini_buf *);
typedef void (*outputThreadCallback_t)(struct gemini *, struct msm_gemini_buf *);

int gemini_lib_init(int **fdOut,
					eventThreadCallback_t eventThreadCallback,
					inputThreadCallback_t inputThreadCallback,
					outputThreadCallback_t outputThreadCallback);

void gemini_lib_release(struct gemini *lib);

int gemini_lib_wait_done(struct gemini *lib);

int gemini_lib_encode(struct gemini* lib);

int gemini_lib_stop(struct gemini *lib, int lazyStop);

void gemini_lib_wait_thread_ready(struct gemini *lib, pthread_t *tid);

void gemini_lib_send_thread_ready(struct gemini *lib, struct workerThread *thread);

int gemini_lib_hw_config(struct gemini *lib,
						const struct gemini_input_cfg* inputCfg,
						const uint8_t* hw_we_cfg_params,
						const struct gemini_hw_cfg *pHwCfg,
						const struct gemini_op_cfg *pOpCfg);

void* do_mmap(size_t allocSize, int *pmemFd);
int do_munmap(int pmemFd, void* memory, size_t allocSize);

void gemini_lib_hw_create_huffman_table(unsigned char *table, unsigned char *table2, uint16_t *table3, bool flag);

void gemini_lib_hw_create_huffman_tables(const struct gemini_hw_cfg* huffmanTable, uint16_t* huffmanValues1, uint16_t* huffmanValues2, uint16_t* huffmanValues3, uint16_t* huffmanValues4);

int gemini_lib_input_buf_enq(struct gemini *lib, struct msm_gemini_buf *buf);
int gemini_lib_output_buf_enq(struct gemini *lib, struct msm_gemini_buf *buf);

int gemini_app_calc_param(struct gemini_app_param *appParam, unsigned int param1, unsigned int jpegQuality, int param3, unsigned int param4, int param5, int param6);

void gemini_lib_hw_get_version(struct msm_gemini_hw_cmd *out);
struct msm_gemini_hw_cmds* gemini_lib_hw_set_filesize_ctrl(const struct gemini_filesize_ctrl_cfg* fszCtrlCfg);
struct msm_gemini_hw_cmds* gemini_lib_hw_we_cfg(const uint8_t* array);
struct msm_gemini_hw_cmds* gemini_lib_hw_start(const uint8_t* arr);
struct msm_gemini_hw_cmds* gemini_lib_hw_restart_marker_set(uint16_t restartMarker);
struct msm_gemini_hw_cmds* gemini_lib_hw_stop_realtime(bool flag);
struct msm_gemini_hw_cmds* gemini_lib_hw_fe_cfg(const struct gemini_input_cfg *p_input_cfg);
struct msm_gemini_hw_cmds* gemini_lib_hw_stop_offline();
struct msm_gemini_hw_cmds* gemini_lib_hw_stop_realtime(bool flag);
struct msm_gemini_hw_cmds* gemini_lib_hw_stop(const unsigned char* cmdType, bool flagRealtime);
struct msm_gemini_hw_cmds* gemini_lib_hw_pipeline_cfg(const struct gemini_pipeline_cfg *pIn);
struct msm_gemini_hw_cmds* gemini_lib_hw_op_cfg(const struct gemini_op_cfg* opCfg, const struct gemini_output_cfg* outCfg);
struct msm_gemini_hw_cmds* gemini_lib_hw_read_quant_tables();
struct msm_gemini_hw_cmds* gemini_lib_hw_set_quant_tables(const uint8_t* table1, const uint8_t* table2);
struct msm_gemini_hw_cmds* gemini_lib_hw_set_huffman_tables(const uint16_t* table1, const uint16_t* table2, const uint16_t* table3, const uint16_t* table4);
