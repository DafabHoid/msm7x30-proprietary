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

#include "gemini.h"
#include <media/msm_gemini.h> // Kernel header
#include <stdlib.h>
#include <string.h>
#include <log/log.h>

/** Allocate a msm_gemini_hw_cmds struct, with the param cmds as payload in
 * the hw_cmd[] member. 
 * @param size The size in bytes of the msm_gemini_hw_cmds.hw_cmd[] array, that
 *             needs to be allocated. Needs to be a multiple of
 *             sizeof(struct msm_gemini_hw_cmd).
 * @param cmds A pointer to an array of msm_gemini_hw_cmd structs, which should
 *             be copied to the newly allocated structure. Parameter size should
 *             be sizeof(struct msm_gemini_hw_cmd) * (array length of cmds).
 *             If cmds is NULL, no copy will be performed, but just the
 *             allocation.
 */
static struct msm_gemini_hw_cmds* makeHwCmds(size_t size, const struct msm_gemini_hw_cmd *cmds)
{
	struct msm_gemini_hw_cmds *ret = malloc(size + sizeof(ret->m));
	if (ret)
	{
		ret->m = size / sizeof(*cmds);
		if (cmds)
			memcpy(ret->hw_cmd, cmds, size);
	}
	return ret;
};

static const struct msm_gemini_hw_cmd g_hw_get_version_cmd =
{
	.type    = MSM_GEMINI_HW_CMD_TYPE_READ,
	.n       = 1,
	.offset  = 0x0,
	.mask    = 0xFFFFF,
	.data    = 0,
};

void gemini_lib_hw_get_version(struct msm_gemini_hw_cmd *out)
{
	memcpy(out, &g_hw_get_version_cmd, sizeof(g_hw_get_version_cmd));
}

static const struct msm_gemini_hw_cmd g_hw_set_filesize_ctrl_cmds[] = {
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x110,
		.mask   = 0x11F,
		.data   = 0,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x114,
		.mask   = 0xFFFFFFFF,
		.data   = 0,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x118,
		.mask   = 0xFFFFFFFF,
		.data   = 0,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x11C,
		.mask   = 0xFFFFFFFF,
		.data   = 0,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x120,
		.mask   = 0xFFFFFFFF,
		.data   = 0,
	},
};

struct msm_gemini_hw_cmds* gemini_lib_hw_set_filesize_ctrl(const struct gemini_filesize_ctrl_cfg* fcfg)
{
	struct msm_gemini_hw_cmds *result = makeHwCmds(
			sizeof(g_hw_set_filesize_ctrl_cmds),
			g_hw_set_filesize_ctrl_cmds);

	if (result)
	{
		const uint8_t* data = fcfg->data;
		result->hw_cmd[0].data = (fcfg->value - 1) & 0x1F;
		result->hw_cmd[1].data = (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
		result->hw_cmd[2].data = (data[7] << 24) | (data[6] << 16) | (data[5] << 8) | data[4];
		result->hw_cmd[3].data = (data[11] << 24) | (data[10] << 16) | (data[9] << 8) | data[8];
		result->hw_cmd[4].data = (data[15] << 24) | (data[14] << 16) | (data[13] << 8) | data[12];
	}
	return result;
}

static const struct msm_gemini_hw_cmd g_hw_we_cfg_cmds[] = {
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x98,
		.mask   = 0xF0F37,
		.data   = 0,
	},
};

struct msm_gemini_hw_cmds* gemini_lib_hw_we_cfg(const uint8_t* arr)
{
	struct msm_gemini_hw_cmds* result = makeHwCmds(
			sizeof(g_hw_we_cfg_cmds),
			g_hw_we_cfg_cmds);

	if (result)
	{
		result->hw_cmd[0].data = ((arr[0] << 4) & 0x30) | (arr[1] & 7);
	}
	return result;
}

static const struct msm_gemini_hw_cmd g_hw_start_cmds[] = {
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x14,
		.mask   = 0xFFFFFFFF,
		.data   = 0xFFFFFFFF,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0xF0,
		.mask   = 0x1,
		.data   = 0x1,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x94,
		.mask   = 0x3,
		.data   = 0x3,
	},
};

struct msm_gemini_hw_cmds* gemini_lib_hw_start(const uint8_t* arr)
{
	struct msm_gemini_hw_cmds* result = makeHwCmds(
			sizeof(g_hw_start_cmds),
			g_hw_start_cmds);

	if (result)
	{
		if (arr[0] == 0)
		{
			result->hw_cmd[2].offset = 0xC;
			result->hw_cmd[2].data = 0x1;
		}
	}
	return result;
}

static const struct msm_gemini_hw_cmd g_hw_restart_marker_set_cmds[] = {
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE_OR,
		.n      = 1,
		.offset = 0xF4,
		.mask   = 0xFFFF,
		.data   = 0,
	},
};

struct msm_gemini_hw_cmds* gemini_lib_hw_restart_marker_set(uint16_t restartMarker)
{
	struct msm_gemini_hw_cmds* result = makeHwCmds(
			sizeof(g_hw_restart_marker_set_cmds),
			g_hw_restart_marker_set_cmds);

	if (result)
	{
		result->hw_cmd[0].data = restartMarker;
	}
	return result;
}

static const struct msm_gemini_hw_cmd g_hw_fe_cfg_cmds[] = {
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x38,
		.mask   = 0xF0077,
		.data   = 0,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x3C,
		.mask   = 0x1FF01FF,
		.data   = 0,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x40,
		.mask   = 0x107FF,
		.data   = 0,
	},
};

struct msm_gemini_hw_cmds* gemini_lib_hw_fe_cfg(const struct gemini_input_cfg *p_input_cfg)
{	
	struct msm_gemini_hw_cmds *result = makeHwCmds(
			sizeof(g_hw_fe_cfg_cmds),
			g_hw_fe_cfg_cmds);
	if (result)
	{
		result->hw_cmd[0].data = ((p_input_cfg->params[0] << 6) & 0x40)
				| ((p_input_cfg->params[1] << 4) & 0x30)
				| (p_input_cfg->params[2] & 7);

		ALOGE("p_input_cfg->frame_height_mcus %d, p_input_cfg->frame_width_mcus %d\n",
			p_input_cfg->frame_height_mcus,
			p_input_cfg->frame_width_mcus);

		result->hw_cmd[1].data = (((p_input_cfg->frame_height_mcus - 1) << 16) & 0x1FF0000)
				| ((p_input_cfg->frame_width_mcus - 1) & 0x1FF);
		
		int data_byte1, data_byte0;
		if (p_input_cfg->inputFormat - 1 <= 1)
		{
			data_byte0 = 1;
			data_byte1 = 2;
		}
		else
		{
			data_byte0 = 3;
			data_byte1 = 3;
		}
		result->hw_cmd[2].data = (((data_byte1 - 1) << 8) & 0x700) | data_byte0;
	}
	return result;
}

static const struct msm_gemini_hw_cmd g_hw_op_cfg_cmds[] = {
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x44,
		.mask   = 0x3,
		.data   = 0,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x48,
		.mask   = 0x3FFFFFF,
		.data   = 0,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x4C,
		.mask   = 0x3FFFFFF,
		.data   = 0,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x50,
		.mask   = 0x3FFFFFF,
		.data   = 0,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x54,
		.mask   = 0x3FFFFFF,
		.data   = 0,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x58,
		.mask   = 0x33F1F1F,
		.data   = 0x108,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x5C,
		.mask   = 0xFFFF,
		.data   = 0,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x60,
		.mask   = 0xFFFFFFFF,
		.data   = 0,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x64,
		.mask   = 0xFFFFFFFF,
		.data   = 0,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x68,
		.mask   = 0xFFFFFFFF,
		.data   = 0,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x6C,
		.mask   = 0xFFFFFFFF,
		.data   = 0,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x70,
		.mask   = 0xFFFFFFFF,
		.data   = 0,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x74,
		.mask   = 0xFFFFFFFF,
		.data   = 0,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x78,
		.mask   = 0xFFFFFFFF,
		.data   = 0,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x7C,
		.mask   = 0xFFFFFFFF,
		.data   = 0,
	},
};

static const unsigned char hw_burstmask_table_index[] = {2, 3, 1, 0};

static const uint32_t hw_burstmask_table[44] = {
	     0xF0F,     0xFFFF, 0xFF0000FF, 0xFF0000FF,
	0xFF0000FF, 0xFF0000FF,          0, 0xFF0000FF,
	         0, 0xFF0000FF, 0xF0F00F0F, 0xF0F00F0F,
	0xF0F00F0F, 0xF0F00F0F,          0, 0xF0F00F0F,
	         0, 0xF0F00F0F,      0x303,      0x303,
	     0x301,   0xF0000F,   0xF0000F,   0xF0000F,
	0xF0000F00, 0xF0000F00,          0,          0,
	         0,          0,          0,          0,
	         0,  0xC0C0303,  0xC0C0303,  0xC0C0303,
	0xC0C03030, 0xC0C03030, 0xC0C03030,          0,
	         0,          0,          0,          0,
};

struct msm_gemini_hw_cmds* gemini_lib_hw_op_cfg(const struct gemini_op_cfg* opCfg, const struct gemini_output_cfg* outCfg)
{
	struct msm_gemini_hw_cmds* result = makeHwCmds(
			sizeof(g_hw_op_cfg_cmds),
			g_hw_op_cfg_cmds);

	if (result)
	{
		unsigned int val0, val1, val2, val3, val4, val5, val6, val7, val8;
		result->hw_cmd[0].data = opCfg->value & 3;
		if (opCfg->op_mode == MSM_GEMINI_MODE_REALTIME_ENCODE)
		{
			unsigned int data0, data1, data2, data3, data4;
			unsigned int data5_byte0, data5_byte1, data5_byte2, data5_byte3;
			unsigned int factor;

			data5_byte3 = opCfg->value;
			data0 = outCfg->inputFormat - 2 > 1 ?  1 : 2;
			if (outCfg->inputFormat == 1 || outCfg->inputFormat == 3) {
				factor = 2;
			} else {
				factor = 1;
			}
			data1 = 0;
			data2 = 0;
			data3 = (outCfg->frame_width_mcus * (8 * factor - 1) + 1) * (data0 << 3);
			data4 = 16 * (7 * outCfg->frame_width_mcus + 1);
			data5_byte0 = 8;
			data5_byte1 = 1;
			data5_byte2 = 0;
			
			if (opCfg->value == 1)
			{
				unsigned int tmp = (outCfg->frame_height_mcus + 0x1FFFFFFF) * 8 * outCfg->frame_width_mcus;
				data1 = (data0 << 3) * (outCfg->frame_width_mcus + 0x1FFFFFFF);
				data2 = 16 * (outCfg->frame_width_mcus + 0x1FFFFFFF);
				data3 = (tmp * factor + 1) * (data0 << 3);
				data4 = 16 * (tmp + 1);
				data5_byte0 = 31;
				data5_byte1 = 8;
				data5_byte2 = 7;
			}
			else if (opCfg->value == 2)
			{
				unsigned int tmp = (outCfg->frame_height_mcus + 0x1FFFFFFF) * 8 * outCfg->frame_width_mcus;
				data1 = (outCfg->frame_width_mcus + 0x1FFFFFFF + tmp * factor) * (data0 << 3);
				data2 = 16 * (tmp + outCfg->frame_width_mcus + 0x1FFFFFFF);
				data5_byte0 = 24;
				data5_byte1 = 31;
				data5_byte2 = 63;
			}
			else if (opCfg->value == 3)
			{
				unsigned int tmp = outCfg->frame_width_mcus * (outCfg->frame_height_mcus + 0x1FFFFFFF);
				data1 = tmp * factor * (data0 << 6);
				data2 = tmp * 128;
				data3 = (8 * tmp * factor + 1) * (data0 << 3);
				data4 = data2 + 16;
				data5_byte0 = 1;
				data5_byte1 = 24;
				data5_byte2 = 56;
			}
			else
			{
				data5_byte3 = 0;
			}
			const unsigned int HW_OP_CFG_DATA_MASK = 0x3FFFFFF;
			result->hw_cmd[1].data = data1 & HW_OP_CFG_DATA_MASK;
			result->hw_cmd[2].data = data2 & HW_OP_CFG_DATA_MASK;
			result->hw_cmd[3].data = data3 & HW_OP_CFG_DATA_MASK;
			result->hw_cmd[4].data = data4 & HW_OP_CFG_DATA_MASK;
			result->hw_cmd[5].data = 0
					| ((data5_byte3 << 24) & HW_OP_CFG_DATA_MASK)
					| (data5_byte2 << 16) 
					| (data5_byte1 << 8)
					| data5_byte0;
			
			const unsigned int* burstmask_table = &hw_burstmask_table[hw_burstmask_table_index[outCfg->inputFormat]];
			val0 = burstmask_table[18];
			val1 = burstmask_table[21];
			val2 = burstmask_table[24];
			val3 = burstmask_table[27];
			val4 = burstmask_table[30];
			val5 = burstmask_table[33];
			val6 = burstmask_table[36];
			val7 = burstmask_table[39];
			val8 = burstmask_table[42];
		}
		else
		{
			const unsigned int* burstmask_table = hw_burstmask_table + (outCfg->param != 1 ? 1 : 0);
			val0 = burstmask_table[0];
			val1 = burstmask_table[2];
			val2 = burstmask_table[4];
			val3 = burstmask_table[6];
			val4 = burstmask_table[8];
			val5 = burstmask_table[10];
			val6 = burstmask_table[12];
			val7 = burstmask_table[14];
			val8 = burstmask_table[16];
		}
		result->hw_cmd[6].data = val0;
		result->hw_cmd[7].data = val1;
		result->hw_cmd[8].data = val2;
		result->hw_cmd[9].data = val3;
		result->hw_cmd[10].data = val4;
		result->hw_cmd[11].data = val5;
		result->hw_cmd[12].data = val6;
		result->hw_cmd[13].data = val7;
		result->hw_cmd[14].data = val8;
	}
	return result;
}

static const struct msm_gemini_hw_cmd g_hw_stop_realtime_cmds[] = {
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0xC,
		.mask   = 0x3,
		.data   = 0x3,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x24,
		.mask   = 0x1,
		.data   = 0x1,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_UWAIT,
		.n      = 0xFFF,
		.offset = 0x28,
		.mask   = 0x3,
		.data   = 0x3,
	},
};

struct msm_gemini_hw_cmds* gemini_lib_hw_stop_realtime(bool flag)
{
	struct msm_gemini_hw_cmds* result = makeHwCmds(
			sizeof(g_hw_stop_realtime_cmds),
			g_hw_stop_realtime_cmds);

	if (result)
	{
		if (flag)
		{
			result->hw_cmd[0].data = 0;
		}
	}
	return result;
}

static const struct msm_gemini_hw_cmd g_hw_stop_offline_cmds[] = {
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x24,
		.mask   = 0x1,
		.data   = 0x1,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_UWAIT,
		.n      = 0xFFF,
		.offset = 0x28,
		.mask   = 0x3,
		.data   = 0x3,
	},
};

struct msm_gemini_hw_cmds* gemini_lib_hw_stop_offline()
{
	return makeHwCmds(sizeof(g_hw_stop_offline_cmds), g_hw_stop_offline_cmds);
}

struct msm_gemini_hw_cmds* gemini_lib_hw_stop(const unsigned char* cmdType, bool flagRealtime)
{
	if (*cmdType)
		return gemini_lib_hw_stop_offline();
	else
		return gemini_lib_hw_stop_realtime(flagRealtime);
}

static const struct msm_gemini_hw_cmd g_hw_pipeline_cfg_cmds[] = {
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x8,
		.mask   = 0x7F775FF,
		.data   = 0,
	},
};

#define HWIO_JPEG_CFG_JPEG_FORMAT_BMSK 0x1800000
#define HWIO_JPEG_CFG_JPEG_FORMAT_SHFT 23

struct msm_gemini_hw_cmds* gemini_lib_hw_pipeline_cfg(const struct gemini_pipeline_cfg *pIn)
{
	struct msm_gemini_hw_cmds *result = makeHwCmds(
			sizeof(g_hw_pipeline_cfg_cmds),
			g_hw_pipeline_cfg_cmds);

	if (result)
	{
		ALOGE("pIn->nInputFormat = %d, HWIO_JPEG_CFG_JPEG_FORMAT_SHFT = %d, HWIO_JPEG_CFG_JPEG_FORMAT_BMSK = %d\n",
			pIn->inputFormat,
			HWIO_JPEG_CFG_JPEG_FORMAT_SHFT,
			HWIO_JPEG_CFG_JPEG_FORMAT_BMSK);

		uint32_t mask = pIn->op_mode != MSM_GEMINI_MODE_REALTIME_ENCODE ? 0x2000000u : 0;
		result->hw_cmd[0].data = 0
				| mask
				| 0x61FB
				| ((pIn->inputFormat << HWIO_JPEG_CFG_JPEG_FORMAT_SHFT) & HWIO_JPEG_CFG_JPEG_FORMAT_BMSK)
				| ((pIn->data[0] << 22) & 0x400000)
				| ((pIn->data[1] << 21) & 0x200000)
				| ((pIn->data[2] << 20) & 0x100000)
				| ((pIn->data[3] << 10) & 0x400)
				| (4 * pIn->data[4] & 4);
	}
	return result;
}

static const struct msm_gemini_hw_cmd g_hw_read_quant_tables_cmds[] = {
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x128,
		.mask   = 0x3FF,
		.data   = 0,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x124,
		.mask   = 0x7,
		.data   = 0x5,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x124,
		.mask   = 0x7,
		.data   = 0,
	},
};

#define HW_QUANT_TABLE_SIZE 64

struct msm_gemini_hw_cmds* gemini_lib_hw_read_quant_tables()
{
	struct msm_gemini_hw_cmds *result = makeHwCmds(
			sizeof(g_hw_read_quant_tables_cmds) + HW_QUANT_TABLE_SIZE * 2 * sizeof(struct msm_gemini_hw_cmd), NULL);
	if(result)
	{
		memcpy(result->hw_cmd, g_hw_read_quant_tables_cmds, sizeof(g_hw_read_quant_tables_cmds));

		const int FIRST_CMD_COUNT =
				sizeof(g_hw_read_quant_tables_cmds) / sizeof(g_hw_read_quant_tables_cmds[0]);

		struct msm_gemini_hw_cmd* cmd = &result->hw_cmd[FIRST_CMD_COUNT - 1];
		for (; cmd < &result->hw_cmd[HW_QUANT_TABLE_SIZE*2 + FIRST_CMD_COUNT - 1]; ++cmd)
		{
			cmd->type = MSM_GEMINI_HW_CMD_TYPE_READ;
			cmd->n = 1;
			cmd->offset = 0x12C;
			cmd->mask = 0xFFFFFFFF;
		}
		// cmd is now &result->hw_cmd[HW_QUANT_TABLE_SIZE*2 + FIRST_CMD_COUNT - 1]
		cmd->type = g_hw_read_quant_tables_cmds[2].type;
		cmd->n = g_hw_read_quant_tables_cmds[2].n;
		cmd->offset = g_hw_read_quant_tables_cmds[2].offset;
		cmd->mask = g_hw_read_quant_tables_cmds[2].mask;
		cmd->data = g_hw_read_quant_tables_cmds[2].data;
	}
	return result;
}

static const struct msm_gemini_hw_cmd g_hw_set_quant_tables_cmds[] = {
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x128,
		.mask   = 0x3FF,
		.data   = 0,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x124,
		.mask   = 0x7,
		.data   = 0x5,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x124,
		.mask   = 0x7,
		.data   = 0,
	},
};

struct msm_gemini_hw_cmds* gemini_lib_hw_set_quant_tables(const uint8_t* table1, const uint8_t* table2)
{
	struct msm_gemini_hw_cmds *result = makeHwCmds(
			sizeof(g_hw_set_quant_tables_cmds) + 2 * HW_QUANT_TABLE_SIZE * sizeof(struct msm_gemini_hw_cmd), NULL);
	if(result)
	{
		memcpy(result->hw_cmd, g_hw_set_quant_tables_cmds, sizeof(g_hw_set_quant_tables_cmds));

		const int FIRST_CMD_COUNT =
				sizeof(g_hw_set_quant_tables_cmds) / sizeof(g_hw_set_quant_tables_cmds[0]);

		struct msm_gemini_hw_cmd* cmd = &result->hw_cmd[FIRST_CMD_COUNT - 1];
		for (int i = 0; i < HW_QUANT_TABLE_SIZE; ++i) // table 1
		{
			unsigned int tableValue = table1[i];
			cmd->type = MSM_GEMINI_HW_CMD_TYPE_WRITE;
			cmd->n = 1;
			cmd->offset = 0x12C;
			cmd->mask = 0xFFFFFFFF;
			cmd->data = tableValue > 1 ? (0x10000 / tableValue) : 0xFFFF;
			++cmd;
		}
		for (int i = 0; i < HW_QUANT_TABLE_SIZE; ++i) // table 2
		{
			unsigned int tableValue = table2[i];
			cmd->type = MSM_GEMINI_HW_CMD_TYPE_WRITE;
			cmd->n = 1;
			cmd->offset = 0x12C;
			cmd->mask = 0xFFFFFFFF;
			cmd->data = tableValue > 1 ? (0x10000 / tableValue) : 0xFFFF;
			++cmd;
		}
		// cmd is now &result->hw_cmd[2 * HW_QUANT_TABLE_SIZE + FIRST_CMD_COUNT - 1]
		cmd->type = g_hw_set_quant_tables_cmds[2].type;
		cmd->n = g_hw_set_quant_tables_cmds[2].n;
		cmd->offset = g_hw_set_quant_tables_cmds[2].offset;
		cmd->mask = g_hw_set_quant_tables_cmds[2].mask;
		cmd->data = g_hw_set_quant_tables_cmds[2].data;
	}
	return result;
}

static const struct msm_gemini_hw_cmd g_hw_set_huffman_tables_cmds[] = {
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE_OR,
		.n      = 1,
		.offset = 0xF4,
		.mask   = 0x1FFFF,
		.data   = 0,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x124,
		.mask   = 0x7,
		.data   = 0x6,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x128,
		.mask   = 0x3FF,
		.data   = 0,
	},
	{
		.type   = MSM_GEMINI_HW_CMD_TYPE_WRITE,
		.n      = 1,
		.offset = 0x124,
		.mask   = 0x7,
		.data   = 0,
	},
};

static void fillSetHuffmanTablesCmds_24(struct msm_gemini_hw_cmd* cmd, const uint16_t* table, unsigned int param)
{
	for (int i = 0; i < 12; ++i)
	{
		uint16_t huffman1 = table[2 * i];
		uint16_t huffman2 = table[2 * i + 1];
		cmd[0].type = MSM_GEMINI_HW_CMD_TYPE_WRITE;
		cmd[0].n    = 1;
		cmd[0].offset = 0x128;
		cmd[0].mask = 0x3FF;
		cmd[0].data = param;
		cmd[1].type = MSM_GEMINI_HW_CMD_TYPE_WRITE;
		cmd[1].n    = 1;
		cmd[1].offset = 0x12C;
		cmd[1].mask = 0xFFFFFFFF;
		cmd[1].data = ((huffman1 + i - 1) << 16) + (huffman2 << (16 - huffman1));
		param += 64;
		cmd += 2;
	}
}

static void fillSetHuffmanTablesCmds_352(struct msm_gemini_hw_cmd* cmd, const uint16_t* table)
{
	for (int i = 0; i < 352; ++i)
	{
		uint16_t huffman1 = table[2 * i];
		uint16_t huffman2 = table[2 * i + 1];
		cmd[0].type = MSM_GEMINI_HW_CMD_TYPE_WRITE;
		cmd[0].n    = 1;
		cmd[0].offset = 0x128;
		cmd[0].mask = 0x3FF;
		cmd[0].data = 4 * i;
		cmd[1].type = MSM_GEMINI_HW_CMD_TYPE_WRITE;
		cmd[1].n    = 1;
		cmd[1].offset = 0x12C;
		cmd[1].mask = 0xFFFFFFFF;
		cmd[1].data = ((((uint8_t)(huffman1 - 1) + (i >> 4)) & 0x1F) << 16) + (huffman2 << (16 - huffman1));
		cmd += 2;
	}
}

struct msm_gemini_hw_cmds* gemini_lib_hw_set_huffman_tables(const uint16_t* table1, const uint16_t* table2, const uint16_t* table3, const uint16_t* table4)
{
	struct msm_gemini_hw_cmds* result = makeHwCmds(
			sizeof(g_hw_set_huffman_tables_cmds) + 752 * sizeof(struct msm_gemini_hw_cmd),
			NULL);

	if (result)
	{
		memcpy(&result->hw_cmd[0], g_hw_set_huffman_tables_cmds, sizeof(g_hw_set_huffman_tables_cmds));
		if (table1 && table2 && table3 && table4)
		{
			result->hw_cmd[0].data = 0x10000;
			struct msm_gemini_hw_cmd* cmd = &result->hw_cmd[3];
			fillSetHuffmanTablesCmds_24(cmd, table1, 2);
			cmd += 24;
			fillSetHuffmanTablesCmds_24(cmd, table2, 3);
			cmd += 24;
			fillSetHuffmanTablesCmds_352(cmd, table3);
			cmd += 352;
			fillSetHuffmanTablesCmds_352(cmd, table4);
			cmd += 352;
			memcpy(cmd, &g_hw_set_huffman_tables_cmds[3], sizeof(struct msm_gemini_hw_cmd));
		}
	}
	return result;
}
