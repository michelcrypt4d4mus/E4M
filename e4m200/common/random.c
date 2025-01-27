/* Copyright (C) 1998-99 Paul Le Roux. All rights reserved. Please see the
   file license.txt for full license details. paulca@rocketmail.com */

#include "e4mdefs.h"

#include "crypto.h"
#include "crc.h"
#include "random.h"

/* random management defines & pool pointers */
#define POOLSIZE 256
#define RANDOMPOOL_ALLOCSIZE	( ( POOLSIZE + SHA_DIGESTSIZE - 1 ) \
	/ SHA_DIGESTSIZE ) * SHA_DIGESTSIZE

unsigned char *pRandPool = NULL;
int nRandIndex = 0;

/* Macro to add a single byte to the pool */
#define RandaddByte(x) {\
	if (nRandIndex==POOLSIZE) \
		Randmix(); \
	pRandPool[nRandIndex] = (unsigned char) ((unsigned char)x + pRandPool[nRandIndex]); \
	nRandIndex++; \
	}


/* Macro to add four bytes to the pool */
#define RandaddLong(quantum) { \
	RandaddByte((unsigned long) quantum); \
	RandaddByte((unsigned long) quantum >> 8); \
	RandaddByte((unsigned long) quantum >> 16); \
	RandaddByte((unsigned long) quantum >> 24); \
	}

HHOOK hMouse = NULL;		/* Mouse hook for the random number generator */
HHOOK hKeyboard = NULL;		/* Keyboard hook for the random number
				   generator */

/* Variables for thread control, the thread is used to gather up info about
   the system in in the background */
CRITICAL_SECTION critRandProt;	/* The critical section */
BOOL volatile bThreadTerminate = FALSE;	/* This variable is shared among
					   thread's so its made volatile */
BOOL bDidSlowPoll = FALSE;	/* We do the slow poll only once */

/* Network library handle for the slowPollWinNT function */
HANDLE hNetAPI32 = NULL;

BOOL bRandDidInit = FALSE;

/* Init the random number generator, setup the hooks, and start the thread */
int
Randinit ()
{
  HANDLE threadID;

  InitializeCriticalSection (&critRandProt);

  bRandDidInit = TRUE;

  pRandPool = (unsigned char *) e4malloc (RANDOMPOOL_ALLOCSIZE);
  if (pRandPool==NULL)
    goto error;
  else
    memset (pRandPool, 0, RANDOMPOOL_ALLOCSIZE);

  hMouse = SetWindowsHookEx (WH_MOUSE, &MouseProc, NULL, GetCurrentThreadId ());
  hKeyboard = SetWindowsHookEx (WH_KEYBOARD, &KeyboardProc, NULL, GetCurrentThreadId ());
  if (hMouse==0 || hKeyboard==0)
    goto error;

  threadID = (HANDLE) _beginthread (ThreadSafeThreadFunction, 8192, NULL);
  if (threadID == (HANDLE) - 1)
    goto error;

  return 0;

error:
  Randfree ();
  return 1;
}

/* Close everything down, including the thread which is closed down by
   setting a flag which eventually causes the thread function to exit */
void
Randfree ()
{
  if (bRandDidInit == FALSE)
    return;

  EnterCriticalSection (&critRandProt);

  if (hMouse!=0)
    UnhookWindowsHookEx (hMouse);
  if (hKeyboard!=0)
    UnhookWindowsHookEx (hKeyboard);

  bThreadTerminate = TRUE;

  LeaveCriticalSection (&critRandProt);

  for (;;)
    {
      Sleep (250);
      EnterCriticalSection (&critRandProt);
      if (bThreadTerminate == FALSE)
	{
	  LeaveCriticalSection (&critRandProt);
	  break;
	}
      LeaveCriticalSection (&critRandProt);
    }

  if (pRandPool!=NULL)
    {
      burn (pRandPool, RANDOMPOOL_ALLOCSIZE);
      e4mfree (pRandPool);
      pRandPool = NULL;
    }

  if (hNetAPI32!=0)
    {
      FreeLibrary (hNetAPI32);
      hNetAPI32 = NULL;
    }

  hMouse = NULL;
  hKeyboard = NULL;
  nRandIndex = 0;
  bThreadTerminate = FALSE;
  DeleteCriticalSection (&critRandProt);
  bRandDidInit = FALSE;
}

/* Mix random pool with the hash function */
void
Randmix ()
{
  int i;

  for (i = 0; i < POOLSIZE; i += SHA_DIGESTSIZE)
    {
      unsigned char inputBuffer[SHA_BLOCKSIZE];
      int j;

      /* Copy SHA_BLOCKSIZE bytes from the circular buffer into the hash data
         buffer, hash the data, and copy the result back into the random pool */
      for (j = 0; j < SHA_BLOCKSIZE; j++)
	inputBuffer[j] = pRandPool[(i + j) % POOLSIZE];
      SHA1TRANSFORM ((unsigned long *) (pRandPool + i), inputBuffer);
      memset (inputBuffer, 0, SHA_BLOCKSIZE);
    }

  nRandIndex = 0;
}

/* Add a buffer to the pool */
void
RandaddBuf (void *buf, int len)
{
  int i;
  for (i = 0; i < len; i++)
    {
      RandaddByte (((unsigned char *) buf)[i]);
    }
}

void
RandpeekBytes (char *buf, int len)
{
  EnterCriticalSection (&critRandProt);
  memcpy (buf, pRandPool, len);
  LeaveCriticalSection (&critRandProt);
}

/* Get a certain amount of true random bytes from the pool */
void
RandgetBytes (char *buf, int len)
{
  int i;

  EnterCriticalSection (&critRandProt);

  FastPoll ();

  if (bDidSlowPoll == FALSE)
    {
      bDidSlowPoll = TRUE;
      SlowPollWinNT ();
    }
  /* Then mix the pool */
  Randmix ();

  /* There's never more than POOLSIZE worth of randomess */
  if (len > POOLSIZE)
    len = POOLSIZE;

  /* Extract out the random bytes needed */
  memcpy (buf, pRandPool, len);

  /* Now invert the pool */
  for (i = 0; i < POOLSIZE / 4; i++)
    {
      ((unsigned long *) pRandPool)[i] = ~((unsigned long *) pRandPool)[i];
    }

  /* Now remix the pool, creating the new pool */
  Randmix ();

  LeaveCriticalSection (&critRandProt);
}

/* Capture the mouse, and as long as the event is not the same as the last
   two events :- add a crc of the event, and a crc of the time difference
   between this event and the last + the current time to the pool */
int CALLBACK
MouseProc (int nCode, WPARAM wParam, LPARAM lParam)
{
  static DWORD dwLastTimer;
  static unsigned long lastCrc, lastCrc2;
  MOUSEHOOKSTRUCT *lpMouse = (MOUSEHOOKSTRUCT *) lParam;

  if (nCode < 0)
    return CallNextHookEx (hMouse, nCode, wParam, lParam);
  else
    {
      DWORD dwTimer = GetTickCount ();
      DWORD j = dwLastTimer - dwTimer;
      unsigned long crc = 0L;
      int i;

      dwLastTimer = dwTimer;

      for (i = 0; i < sizeof (MOUSEHOOKSTRUCT); i++)
	{
	  crc = UPDC32 (((unsigned char *) lpMouse)[i], crc);
	}

      if (crc != lastCrc && crc != lastCrc2)
	{
	  unsigned long timeCrc = 0L;

	  for (i = 0; i < 4; i++)
	    {
	      timeCrc = UPDC32 (((unsigned char *) &j)[i], timeCrc);
	    }

	  for (i = 0; i < 4; i++)
	    {
	      timeCrc = UPDC32 (((unsigned char *) &dwTimer)[i], timeCrc);
	    }

	  EnterCriticalSection (&critRandProt);
	  RandaddLong (timeCrc);
	  RandaddLong (crc);
	  LeaveCriticalSection (&critRandProt);
	}
      lastCrc2 = lastCrc;
      lastCrc = crc;

    }
  return 0;
}

/* Capture the keyboard, as long as the event is not the same as the last two
   events :- add a crc of the event to the pool along with a crc of the time
   difference between this event and the last */
int CALLBACK
KeyboardProc (int nCode, WPARAM wParam, LPARAM lParam)
{
  static int lLastKey, lLastKey2;
  static DWORD dwLastTimer;
  int nKey = (lParam & 0x00ff0000) >> 16;
  int nCapture = 0;

  if (nCode < 0)
    return CallNextHookEx (hMouse, nCode, wParam, lParam);

  if ((lParam & 0x0000ffff) == 1 && !(lParam & 0x20000000) &&
      (lParam & 0x80000000))
    {
      if (nKey != lLastKey)
	nCapture = 1;		/* Capture this key */
      else if (nKey != lLastKey2)
	nCapture = 1;		/* Allow for one repeat */
    }
  if (nCapture)
    {
      DWORD dwTimer = GetTickCount ();
      DWORD j = dwLastTimer - dwTimer;
      unsigned long timeCrc = 0L;
      int i;

      dwLastTimer = dwTimer;
      lLastKey2 = lLastKey;
      lLastKey = nKey;

      for (i = 0; i < 4; i++)
	{
	  timeCrc = UPDC32 (((unsigned char *) &j)[i], timeCrc);
	}

      for (i = 0; i < 4; i++)
	{
	  timeCrc = UPDC32 (((unsigned char *) &dwTimer)[i], timeCrc);
	}

      EnterCriticalSection (&critRandProt);
      RandaddLong (lParam);
      RandaddLong (timeCrc);
      LeaveCriticalSection (&critRandProt);
    }
  return 0;
}

/* This is the thread function which will poll the system for randomness */
void
ThreadSafeThreadFunction (void *dummy)
{
  if (dummy);			/* Remove unused parameter warning */

  for (;;)
    {
      EnterCriticalSection (&critRandProt);

      if (bThreadTerminate == TRUE)
	{
	  bThreadTerminate = FALSE;
	  LeaveCriticalSection (&critRandProt);
	  _endthread ();
	}
      else
	{
	  FastPoll ();
	}

      LeaveCriticalSection (&critRandProt);

      Sleep (250);
    }
}

/* Type definitions for function pointers to call NetAPI32 functions */

typedef
  DWORD (WINAPI * NETSTATISTICSGET) (LPWSTR szServer, LPWSTR szService,
				     DWORD dwLevel, DWORD dwOptions,
				     LPBYTE * lpBuffer);
typedef
  DWORD (WINAPI * NETAPIBUFFERSIZE) (LPVOID lpBuffer, LPDWORD cbBuffer);
typedef
  DWORD (WINAPI * NETAPIBUFFERFREE) (LPVOID lpBuffer);

NETSTATISTICSGET pNetStatisticsGet = NULL;
NETAPIBUFFERSIZE pNetApiBufferSize = NULL;
NETAPIBUFFERFREE pNetApiBufferFree = NULL;

/* This is the slowpoll function which gathers up network/hard drive
   performance data for the random pool */
void
SlowPollWinNT (void)
{
  static int isWorkstation = -1;
  PPERF_DATA_BLOCK pPerfData;
  static int cbPerfData = 0x10000;
  HANDLE hDevice;
  LPBYTE lpBuffer;
  DWORD dwSize, status;
  LPWSTR lpszLanW, lpszLanS;
  int nDrive;

  /* Find out whether this is an NT server or workstation if necessary */
  if (isWorkstation == -1)
    {
      HKEY hKey;

      if (RegOpenKeyEx (HKEY_LOCAL_MACHINE,
			"SYSTEM\\CurrentControlSet\\Control\\ProductOptions",
			0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
	  unsigned char szValue[32];
	  dwSize = sizeof (szValue);

	  isWorkstation = TRUE;
	  status = RegQueryValueEx (hKey, "ProductType", 0, NULL,
				    szValue, &dwSize);

	  if (status == ERROR_SUCCESS && stricmp ((char *) szValue, "WinNT"))
	    /* Note: There are (at least) three cases for ProductType: WinNT
	       = NT Workstation, ServerNT = NT Server, LanmanNT = NT Server
	       acting as a Domain Controller */
	    isWorkstation = FALSE;

	  RegCloseKey (hKey);
	}
    }
  /* Initialize the NetAPI32 function pointers if necessary */
  if (hNetAPI32 == NULL)
    {
      /* Obtain a handle to the module containing the Lan Manager functions */
      hNetAPI32 = LoadLibrary ("NETAPI32.DLL");
      if (hNetAPI32 != NULL)
	{
	  /* Now get pointers to the functions */
	  pNetStatisticsGet = (NETSTATISTICSGET) GetProcAddress (hNetAPI32,
							"NetStatisticsGet");
	  pNetApiBufferSize = (NETAPIBUFFERSIZE) GetProcAddress (hNetAPI32,
							"NetApiBufferSize");
	  pNetApiBufferFree = (NETAPIBUFFERFREE) GetProcAddress (hNetAPI32,
							"NetApiBufferFree");

	  /* Make sure we got valid pointers for every NetAPI32 function */
	  if (pNetStatisticsGet == NULL ||
	      pNetApiBufferSize == NULL ||
	      pNetApiBufferFree == NULL)
	    {
	      /* Free the library reference and reset the static handle */
	      FreeLibrary (hNetAPI32);
	      hNetAPI32 = NULL;
	    }
	}
    }

  /* Get network statistics.  Note: Both NT Workstation and NT Server by
     default will be running both the workstation and server services.  The
     heuristic below is probably useful though on the assumption that the
     majority of the network traffic will be via the appropriate service */
  lpszLanW = (LPWSTR) WIDE ("LanmanWorkstation");
  lpszLanS = (LPWSTR) WIDE ("LanmanServer");
  if (hNetAPI32 &&
      pNetStatisticsGet (NULL,
			 isWorkstation ? lpszLanW : lpszLanS,
			 0, 0, &lpBuffer) == 0)
    {
      pNetApiBufferSize (lpBuffer, &dwSize);
      RandaddBuf ((unsigned char *) lpBuffer, dwSize);
      pNetApiBufferFree (lpBuffer);
    }

  /* Get disk I/O statistics for all the hard drives */
  for (nDrive = 0;; nDrive++)
    {
      DISK_PERFORMANCE diskPerformance;
      char szDevice[24];

      /* Check whether we can access this device */
      sprintf (szDevice, "\\\\.\\PhysicalDrive%d", nDrive);
      hDevice = CreateFile (szDevice, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
			    NULL, OPEN_EXISTING, 0, NULL);
      if (hDevice == INVALID_HANDLE_VALUE)
	break;


      /* Note: This only works if you have turned on the disk performance
         counters with 'diskperf -y'.  These counters are off by default */
      if (DeviceIoControl (hDevice, IOCTL_DISK_PERFORMANCE, NULL, 0,
			   &diskPerformance, sizeof (DISK_PERFORMANCE),
			   &dwSize, NULL))
	{
	  RandaddBuf ((unsigned char *) &diskPerformance, dwSize);
	}
      CloseHandle (hDevice);
    }

  /* Get the performance counters */
  pPerfData = (PPERF_DATA_BLOCK) e4malloc (cbPerfData);
  while (pPerfData)
    {
      dwSize = cbPerfData;
      status = RegQueryValueEx (HKEY_PERFORMANCE_DATA, "Global", NULL,
				NULL, (LPBYTE) pPerfData, &dwSize);

      if (status == ERROR_SUCCESS)
	{
	  RegCloseKey (HKEY_PERFORMANCE_DATA);
	  if (memcmp (pPerfData->Signature, WIDE ("PERF"), 8)==0)
	    RandaddBuf (pPerfData, dwSize);
	  e4mfree (pPerfData);
	  pPerfData = NULL;
	}
      else if (status == ERROR_MORE_DATA)
	{
	  cbPerfData += 4096;
	  pPerfData = (PPERF_DATA_BLOCK) realloc (pPerfData, cbPerfData);
	}
    }
}


/* This is the fastpoll function which gathers up info by calling various
   api's */
void
FastPoll (void)
{
  static BOOL addedFixedItems = FALSE;
  FILETIME creationTime, exitTime, kernelTime, userTime;
  DWORD minimumWorkingSetSize, maximumWorkingSetSize;
  LARGE_INTEGER performanceCount;
  MEMORYSTATUS memoryStatus;
  HANDLE handle;
  POINT point;

  /* Get various basic pieces of system information */
  RandaddLong (GetActiveWindow ());	/* Handle of active window */
  RandaddLong (GetCapture ());	/* Handle of window with mouse capture */
  RandaddLong (GetClipboardOwner ());	/* Handle of clipboard owner */
  RandaddLong (GetClipboardViewer ());	/* Handle of start of clpbd.viewer
					   list */
  RandaddLong (GetCurrentProcess ());	/* Pseudohandle of current process */
  RandaddLong (GetCurrentProcessId ());	/* Current process ID */
  RandaddLong (GetCurrentThread ());	/* Pseudohandle of current thread */
  RandaddLong (GetCurrentThreadId ());	/* Current thread ID */
  RandaddLong (GetCurrentTime ());	/* Milliseconds since Windows started */
  RandaddLong (GetDesktopWindow ());	/* Handle of desktop window */
  RandaddLong (GetFocus ());	/* Handle of window with kb.focus */
  RandaddLong (GetInputState ());	/* Whether sys.queue has any events */
  RandaddLong (GetMessagePos ());	/* Cursor pos.for last message */
  RandaddLong (GetMessageTime ());	/* 1 ms time for last message */
  RandaddLong (GetOpenClipboardWindow ());	/* Handle of window with
						   clpbd.open */
  RandaddLong (GetProcessHeap ());	/* Handle of process heap */
  RandaddLong (GetProcessWindowStation ());	/* Handle of procs window
						   station */
  RandaddLong (GetQueueStatus (QS_ALLEVENTS));	/* Types of events in input
						   queue */

  /* Get multiword system information */
  GetCaretPos (&point);		/* Current caret position */
  RandaddBuf ((unsigned char *) &point, sizeof (POINT));
  GetCursorPos (&point);	/* Current mouse cursor position */
  RandaddBuf ((unsigned char *) &point, sizeof (POINT));

  /* Get percent of memory in use, bytes of physical memory, bytes of free
     physical memory, bytes in paging file, free bytes in paging file, user
     bytes of address space, and free user bytes */
  memoryStatus.dwLength = sizeof (MEMORYSTATUS);
  GlobalMemoryStatus (&memoryStatus);
  RandaddBuf ((unsigned char *) &memoryStatus, sizeof (MEMORYSTATUS));

  /* Get thread and process creation time, exit time, time in kernel mode,
     and time in user mode in 100ns intervals */
  handle = GetCurrentThread ();
  GetThreadTimes (handle, &creationTime, &exitTime, &kernelTime, &userTime);
  RandaddBuf ((unsigned char *) &creationTime, sizeof (FILETIME));
  RandaddBuf ((unsigned char *) &exitTime, sizeof (FILETIME));
  RandaddBuf ((unsigned char *) &kernelTime, sizeof (FILETIME));
  RandaddBuf ((unsigned char *) &userTime, sizeof (FILETIME));
  handle = GetCurrentProcess ();
  GetProcessTimes (handle, &creationTime, &exitTime, &kernelTime, &userTime);
  RandaddBuf ((unsigned char *) &creationTime, sizeof (FILETIME));
  RandaddBuf ((unsigned char *) &exitTime, sizeof (FILETIME));
  RandaddBuf ((unsigned char *) &kernelTime, sizeof (FILETIME));
  RandaddBuf ((unsigned char *) &userTime, sizeof (FILETIME));

  /* Get the minimum and maximum working set size for the current process */
  GetProcessWorkingSetSize (handle, &minimumWorkingSetSize,
			    &maximumWorkingSetSize);
  RandaddLong (minimumWorkingSetSize);
  RandaddLong (maximumWorkingSetSize);

  /* The following are fixed for the lifetime of the process so we only add
     them once */
  if (addedFixedItems==0)
    {
      STARTUPINFO startupInfo;

      /* Get name of desktop, console window title, new window position and
         size, window flags, and handles for stdin, stdout, and stderr */
      startupInfo.cb = sizeof (STARTUPINFO);
      GetStartupInfo (&startupInfo);
      RandaddBuf ((unsigned char *) &startupInfo, sizeof (STARTUPINFO));
      addedFixedItems = TRUE;
    }
  /* The docs say QPC can fail if appropriate hardware is not available. It
     works on 486 & Pentium boxes, but hasn't been tested for 386 or RISC
     boxes */
  if (QueryPerformanceCounter (&performanceCount))
    RandaddBuf ((unsigned char *) &performanceCount, sizeof (LARGE_INTEGER));
  else
    {
      /* Millisecond accuracy at best... */
      DWORD dwTicks = GetTickCount ();
      RandaddBuf ((unsigned char *) &dwTicks, sizeof (dwTicks));
    }
}
