FOSS replacements for MSM7x30 proprietariy blobs
================================================

This repository contains sources for several executables and libraries, which are reconstructed from proprietary blobs and try to mimic their binary interface and behaviour as close as possible.
The blobs in question are executables and libraries used in Android ROM images for the MSM7x30 board, especially *ariesve* (Samsung Galaxy S Plus GT-I9001) and *ancora* (Samsung Galaxy W GT-I8150).

These binaries are meant to work as a drop-in replacement for the original closed-source variant and should also be ABI-compatible in a way that allows existing proprietary binary libraries and executables to work with it without any difference (until all existing proprietary binary libraries and
executables have been replaced with open source code, of course).

Please excuse the lack of documentation and code quality, as much of the function interfaces and code have been inspired by the proprietary binary, which lacks debugging information, header files and documentation.

Modules
-------
### rmt_storage
Userspace daemon for the kernel's rmt_storage interface, used to give access to modem partitions like `/boot/modem_fs1` or `ssd` to the ril daemon.

**Note on compiling:** This module depends on the kernel header file `<linux/rmt_storage_client.h>`. It is normally not exported, so if you want to compile rmt_storage in the normal Android build process, you need to locate you kernel sources. There, edit the file `include/linux/Kbuild` and add the line

    header-y += rmt_storage_client.h

somewhere inside.

### libgemini
MSM gemini (JPEG hardware encoder) userspace library, used for example to encode pictures from the camera as JPEG with hardware support from the SoC.
Depends on kernel header file <media/msm_gemini.h>, which should be exported.

### secjpegencoder
Samsung JPEG hardware encoder wrapper library. (Untested, may crash or fail)

Wraps libarccamera.so into a object interface. Further use unknown.
