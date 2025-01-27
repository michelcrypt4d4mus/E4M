/* Copyright (C) 1998-99 Paul Le Roux. All rights reserved. Please see the
   file license.txt for full license details. paulca@rocketmail.com */

/* DeviceIoControl values.

*/

#ifndef CTL_CODE

/* A macro from the NT DDK */

#define CTL_CODE( DeviceType, Function, Method, Access ) ( \
    ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method) \
)

#endif

/* More macros from the NT DDK */

#ifndef METHOD_BUFFERED
#define METHOD_BUFFERED 0
#endif

#ifndef FILE_ANY_ACCESS
#define FILE_ANY_ACCESS 0
#endif

#ifndef FILE_DEVICE_DISK
#define FILE_DEVICE_DISK 0x00000007
#endif

#ifndef IOCTL_DISK_BASE
#define IOCTL_DISK_BASE FILE_DEVICE_DISK
#endif

/* These values originate from the following NT DDK macro :

#define ANYNAME CTL_CODE(IOCTL_DISK_BASE, 0x800, METHOD_BUFFERED, \
   FILE_ANY_ACCESS)

#define ANYNAME2 CTL_CODE(IOCTL_DISK_BASE, 0x801, METHOD_BUFFERED, \
   FILE_ANY_ACCESS)

etc... */

/* Public driver interface codes */

#define MOUNT			466944	/* Mount a volume or partition */
#define MOUNT_LIST		466948	/* Return list of mounted volumes */
#define OPEN_TEST		466952	/* Open a file at ring0 */
#define UNMOUNT			466956	/* Unmount a volume */
#define WIPE_CACHE		466960	/* Wipe the driver password cache */
#define HALT_SYSTEM		466964	/* Halt system; (only NT when
					   compiled with debug) */
#define DRIVER_VERSION		466968	/* Current driver version */
#define RELEASE_TIME_SLICE	466972	/* Release time slice on apps behalf
					   (only Win9x) */
#define MOUNT_LIST_N		466976	/* Get volume info from the device
					   (only Win9x) */
#define DISKIO			466980	/* Read/Write at ring0 for win32 gui
					   (only Win9x) */
#define ALLOW_FAST_SHUTDOWN 466984	/* Fast shutdown under win98 (only
					   Win9x) */
#define UNMOUNT_PENDING		475112	/* Flush the device with this api
					   before sending UNMOUNT */

#define E4M_FIRST_PRIVATE	MOUNT	/* First private control code */
#define E4M_LAST_PRIVATE	UNMOUNT_PENDING	/* Last private control code */

/* Start of driver interface structures, the size of these structures may
   change between versions; so make sure you first send DRIVER_VERSION to
   check that it's the correct device driver */

#pragma pack (push)
#pragma pack(1)

typedef struct
{
	int nReturnCode;	/* Return code back from driver */
	short wszVolume[E4M_MAX_PATH];	/* Volume to be mounted */
	char szPassword[MAX_PASSWORD + 1];	/* User password or SHA1 hash */
	int nPasswordLen;	/* User password length */
	BOOL bCache;		/* Cache passwords in driver */
	int nDosDriveNo;	/* Drive number to mount */
	long time;		/* Time when this volume was mounted */
} MOUNT_STRUCT;

typedef struct
{
	int nReturnCode;	/* Return code back from driver */
	int nDosDriveNo;	/* Drive letter to unmount */
} UNMOUNT_STRUCT;

typedef struct
{
	unsigned long ulMountedDrives;	/* Bitfield of all mounted drive
					   letters */
	short wszVolume[26][64];/* Volume names of mounted volumes */
} MOUNT_LIST_STRUCT;

typedef struct
{
	short wszFileName[E4M_MAX_PATH];	/* Volume to be "open tested" */
	int nReturnCode;	/* Win9x only */
	unsigned long secstart;	/* Win9x only */
	unsigned long seclast;	/* Win9x only */
	unsigned long device;	/* Win9x only */
} OPEN_TEST_STRUCT;

/* Win9x only */
typedef struct
{
	int nReturnCode;	/* Return code back from driver */
	int nDosDriveNo;	/* Drive letter to get info on */
	short wszVolume[E4M_MAX_PATH];	/* Volume names of mounted volumes */
	unsigned long mountfilehandle;	/* Device file handle or 0 */
} MOUNT_LIST_N_STRUCT;

/* Win9x only */
typedef struct
{
	unsigned int devicenum;	/* Ptr to dcb for device */
	unsigned int sectorstart;	/* Start sector */
	unsigned int sectorlen;	/* Number of sectors */
	char *bufferad;		/* Address of buffer */
	int mode;		/* Read=0 or Write=1 */
	int nReturnCode;	/* Return code back from driver */
} DISKIO_STRUCT;


#pragma pack (pop)

#ifdef NT4_DRIVER
#define DRIVER_STR WIDE
#else
#define DRIVER_STR
#endif

/* NT only */

#define NT_MOUNT_PREFIX DRIVER_STR("\\Device\\E4M")
#define NT_ROOT_PREFIX DRIVER_STR("\\Device\\E4MRoot")
#define DOS_MOUNT_PREFIX DRIVER_STR("\\DosDevices\\")
#define DOS_ROOT_PREFIX DRIVER_STR("\\DosDevices\\E4MRoot")
#define WIN32_ROOT_PREFIX DRIVER_STR("\\\\.\\E4MRoot")

/* Win9x only */

#define WIN9X_DRIVER_NAME "\\\\.\\E4M"
