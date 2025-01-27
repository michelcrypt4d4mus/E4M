/* Copyright (C) 1998-99 Paul Le Roux. All rights reserved. Please see the
   file license.txt for full license details. paulca@rocketmail.com */

#define E4M_MAX_PATH                    260	/* Includes the null
						   terminator */
#define SECTOR_SIZE                     512	/* sector size */

#define E4M_OLD_VOLTYPE                 1
#define SFS_VOLTYPE                     2
#define E4M_VOLTYPE2                    4

#define isE4M(x) (x == E4M_OLD_VOLTYPE || x == E4M_VOLTYPE2)

#define BYTES_PER_KB                    1024	/* 1kb = 1024 bytes */
#define BYTES_PER_MB                    1024000	/* On disk 1mb = 1kb * 1000
						   not 1kb^2 */

/* GUI/driver errors */

#define ERR_OS_ERROR                    1
#define ERR_OUTOFMEMORY                 2
#define ERR_PASSWORD_WRONG              3
#define ERR_VOL_FORMAT_BAD              4
#define ERR_BAD_DRIVE_LETTER            5
#define ERR_DRIVE_NOT_FOUND             6
#define ERR_FILES_OPEN                  7
#define ERR_VOL_SIZE_WRONG              8
#define ERR_COMPRESSION_NOT_SUPPORTED   9
#define ERR_PASSWORD_CHANGE_VOL_TYPE    10
#define ERR_PASSWORD_CHANGE_VOL_VERSION 11
#define ERR_VOL_SEEKING                 12
#define ERR_VOL_WRITING                 13
#define ERR_FILES_OPEN_LOCK             14
#define ERR_VOL_READING                 15
#define ERR_DRIVER_VERSION		16

#define ERR_VOL_ALREADY_MOUNTED         32
#define ERR_NO_FREE_SLOTS               33
#define ERR_NO_FREE_DRIVES              34
#define ERR_FILE_OPEN_FAILED            35
#define ERR_VOL_MOUNT_FAILED            36
#define ERR_INVALID_DEVICE              37
#define ERR_ACCESS_DENIED               38

#define MAX_VOLUME_SIZE                 2146435072
#define MIN_VOLUME_SIZE                 19456

#define WIDE(x) (LPWSTR)L##x

#define VERSION_STRING                  "V2.0.1"
#define VERSION_NUM			0x201

#define burn(mem,size) \
	memset(mem,0xff,size); \
	memset(mem,0,size);

#include <string.h>

#pragma intrinsic(memcmp, memcpy, memset, strcat, strcmp, strcpy, strlen)

#ifdef NT4_DRIVER

#pragma warning( disable : 4201 )
#pragma warning( disable : 4214 )
#pragma warning( disable : 4115 )
#pragma warning( disable : 4100 )
#pragma warning( disable : 4101 )
#pragma warning( disable : 4057 )
#pragma warning( disable : 4244 )
#pragma warning( disable : 4514 )
#pragma warning( disable : 4127 )

#include <ntddk.h>		/* Standard header file for nt drivers */
#include <ntdddisk.h>		/* Standard I/O control codes  */
#include <ntiologc.h>

#pragma warning( default : 4201 )
#pragma warning( default : 4214 )
#pragma warning( default : 4115 )
#pragma warning( default : 4100 )
#pragma warning( default : 4101 )
#pragma warning( default : 4057 )
#pragma warning( default : 4244 )
#pragma warning( default : 4127 )

/* #pragma warning( default : 4514 ) this warning remains disabled */

#define e4malloc(size) ((void *) ExAllocatePool( NonPagedPool, size ))

#define e4mfree(memblock) ExFreePool( memblock )

#define DEVICE_DRIVER

#endif				/* NT4_DRIVER */


#ifdef WIN9X_DRIVER

#pragma warning( disable : 4047 )

#include "iosdcls.inc"		/* VMM and IOS headers */

#pragma warning( default : 4047 )

#include <vwin32.h>
#include <winerror.h>
#undef WANTVDXWRAPS
#pragma warning( disable : 4229 )
#include <shell.h>
#pragma warning( default : 4229 )
#pragma hdrstop
#include <malloc.h>
#include <vmm.h>
#include "ifs.h"
#include <dbt.h>
#define MBYTE16 3967
#define UWORD unsigned short
#define UBYTE unsigned char

#define MBYTE16 3967

#define e4malloc(size) _PageAllocate(size % 4096 ? (size/4096)+1 : size/4096,\
	PG_SYS,0,0,0,MBYTE16,NULL,PAGEZEROINIT|PAGEFIXED|PAGECONTIG|PAGEUSEALIGN);

#define e4mfree(memblock) _PageFree(memblock,0)

#define DEVICE_DRIVER

#endif				/* WIN9X_DRIVER */

#ifdef DEVICE_DRIVER

#ifndef BOOL
typedef int BOOL;
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE !TRUE
#endif

/* Define dummies for the drivers */
typedef int HFILE;
typedef unsigned int WPARAM;
typedef unsigned long LPARAM;
#define CALLBACK

#ifndef UINT
typedef unsigned int UINT;
#endif

#ifndef LRESULT
typedef unsigned long LRESULT;
#endif

#else

#define e4malloc malloc
#define e4mfree free

#pragma warning( disable : 4201 )
#pragma warning( disable : 4214 )
#pragma warning( disable : 4115 )
#pragma warning( disable : 4514 )

#include <windows.h>		/* Windows header */
#include <commctrl.h>		/* The common controls */
#include <process.h>		/* Process control */
#include <winioctl.h>
#include <stdio.h>		/* For sprintf */

#pragma warning( default : 4201 )
#pragma warning( default : 4214 )
#pragma warning( default : 4115 )

/* #pragma warning( default : 4514 ) this warning remains disabled */

/* This is needed to fix a bug with VC 5, the TCHAR macro _ttoi64 maps
   incorrectly to atoi64 when it should be _atoi64 */
#define atoi64 _atoi64

#endif				/* DEVICE_DRIVER */

typedef UINT (_stdcall * diskio_f) (int, void *, UINT);

#pragma hdrstop
