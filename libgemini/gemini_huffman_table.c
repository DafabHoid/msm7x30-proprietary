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

void gemini_lib_hw_create_huffman_table(uint8_t *table, uint8_t *table2, uint16_t *table3, bool flag)
{
	uint16_t oneKtable[512];
	
	int width = 256;
	int height = 162;
	if (!flag)
	{
		width = 12;
		height = 12;
	}
	
	uint16_t counter0Start = 0;
	int oneKtableIndex = 0, tableIndex = 0;
	for (uint16_t value0 = 1; value0 != 16; ++value0)
	{
		uint16_t* oneKtablePtr = &oneKtable[2 * oneKtableIndex];
		uint16_t counter0 = counter0Start;
		for (uint8_t j = table[tableIndex]; j != 0; --j)
		{
			*oneKtablePtr++ = value0;
			*oneKtablePtr++ = counter0;
			++counter0;
		}
		oneKtableIndex += table[tableIndex];
		counter0Start = 2 * (counter0Start + table[tableIndex]);
		++tableIndex;
	}
	
	for (int i = 0; i < width; ++i)
	{
		table3[2 * i] = 0;
		table3[2 * i + 1] = 0;
	}
	
	for (int i = 0; i < height; ++i)
	{
		unsigned int table2I = table2[i];
		if (flag)
		{
			table2I = ((table2I << 4) & 0xFF) | (table2I >> 4); // Swap left and right nibble
		}
		table3[2 * table2I] = oneKtable[2 * i];
		table3[2 * table2I + 1] = oneKtable[2 * i + 1];
	}
}

void gemini_lib_hw_create_huffman_tables(const struct gemini_hw_cfg* hwCfg, uint16_t* huffmanValues1, uint16_t* huffmanValues2, uint16_t* huffmanValues3, uint16_t* huffmanValues4)
{
	gemini_lib_hw_create_huffman_table(hwCfg->huffmanTable[0], hwCfg->huffmanTable[0] + 16, huffmanValues1, false);
	gemini_lib_hw_create_huffman_table(hwCfg->huffmanTable[1], hwCfg->huffmanTable[1] + 16, huffmanValues3, true);
	gemini_lib_hw_create_huffman_table(hwCfg->huffmanTable[2], hwCfg->huffmanTable[2] + 16, huffmanValues2, false);
	gemini_lib_hw_create_huffman_table(hwCfg->huffmanTable[3], hwCfg->huffmanTable[3] + 16, huffmanValues4, true);
}
