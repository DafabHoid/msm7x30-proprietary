/* Samsung JPEG hardware encoder wrapper library
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

#define LOG_TAG "SecJpegEncoder"
#include "SecJpegEncoder.h"
#include <utils/Log.h>

// FIXME: Provider proper header files for these
extern "C" void MStreamClose(void* stream);
extern "C" void MStreamSeek(void* stream, int pos, int offset);
extern "C" size_t MStreamRead(void* stream, unsigned char* buffer, size_t length);
extern "C" void* MStreamOpenFromMemoryBlock(int pos, size_t size);
extern "C" void MdBitmapSaveEx(void* amcm, void* stream, int, int* bitmap, int, unsigned int);
extern "C" int AMCM_Create(int, void** dest);
extern "C" void AMCM_Destroy(void* amcm);
extern "C" void AMCM_RegisterEx(void* amcm, unsigned int, unsigned int, int, int, void* func);

namespace android
{

void SecJpegEncoder::init()
{
	std::lock_guard lock(mLock);
	createAMCM();
	mStream = nullptr;
}

void SecJpegEncoder::deInit()
{
	std::lock_guard lock(mLock);
	if (mStream) {
		MStreamClose(mStream);
	}
	destroyAMCM();
}

void SecJpegEncoder::createAMCM()
{
	mAMCM = nullptr;
	if (AMCM_Create(0, &mAMCM) != 0) {
		ALOGE("createAMCM failed!!");
	} else {
		AMCM_RegisterEx(mAMCM, 0x81002202, 0x2000000, 2, 3, &MEncoder_AJL2Create);
	}
}

void SecJpegEncoder::destroyAMCM()
{
	AMCM_Destroy(mAMCM);
	mAMCM = nullptr;
}

bool SecJpegEncoder::getJpeg(unsigned char* buffer, size_t length)
{
	if (mStream) {
		MStreamSeek(mStream, 0, 0);
		MStreamRead(mStream, buffer, length);
		MStreamClose(mStream);
		mStream = nullptr;
		return 1;
	} else {
		ALOGE("m_hStreamSave == NULL, return");
		return 0;
	}
}

bool SecJpegEncoder::encode(android::jpeg_encoder_input& input, android::jpeg_encoder_output& output)
{
	std::lock_guard lock(mLock);
	if (!mAMCM) {
		ALOGE("m_hAMCM == NULL, return");
		output.i1 = 1;
		return false;
	}
	
	mStream = MStreamOpenFromMemoryBlock(0, 3 * input.i1 * input.i2);
	if (!mStream) {
		ALOGE("MStreamOpenFromMemoryBlock failed");
		output.i1 = 1;
		return false;
	}
	
	if (input.i3 != 17) {
		ALOGE("Error: 420sp is only supported.");
		output.i1 = 2;
		return false;
	}
	
	const unsigned int UNKNOWN = 0x70000002u; // FIXME invent name
	
	int i0 = input.i0;
	int i1 = input.i1;
	int i2 = input.i2;
	unsigned int bitmap[9] = {
		UNKNOWN,
		i1, i2, i1, i1, i1, i0,
		i0 + i1 * i2 + 1,
		i0 + i1 * i2,
	};
	MdBitmapSaveEx(mAMCM, mStream, 2, bitmap, input.i4, UNKNOWN);
	MStreamSeek(mStream, 0, 0);
	output.i0 = MStreamGetSize(mStream);
	return true;
}

};
