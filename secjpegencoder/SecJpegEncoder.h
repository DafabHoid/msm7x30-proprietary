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

#include <mutex>

namespace android
{
struct jpeg_encoder_input;
struct jpeg_encoder_output;

class SecJpegEncoder
{
public:
	void init();
	void deInit();
	bool getJpeg(unsigned char* buffer, size_t length);
	bool encode(android::jpeg_encoder_input& input, android::jpeg_encoder_output& output);

private:
	void createAMCM();
	void destroyAMCM();

	void* mAMCM;
	void* mStream;
	std::mutex mLock;
};

};
