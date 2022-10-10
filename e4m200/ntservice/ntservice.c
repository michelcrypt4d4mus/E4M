/* Copyright (C) 1998-99 Paul Le Roux. All rights reserved. Please see the
   file license.txt for full license details. paulca@rocketmail.com */

#include "e4mdefs.h"
#include <process.h>

#include "ntservice.h"

#include "crypto.h"
#include "ntioctl.h"

#include "dismount.h"

/* current status of the service */
SERVICE_STATUS ssStatus;

/* handle for reporting back to the SCM */
SERVICE_STATUS_HANDLE sshStatusHandle = 0;

/* stop event; used at shutdown */
HANDLE hServerStopEvent = NULL;

/* Handle to the device driver */
HANDLE hNTDriver = INVALID_HANDLE_VALUE;

/* main() calls StartServiceCtrlDispatcher to register the main service
   thread.  When the this call returns, the service has stopped, so exit. */

void _CRTAPI1
main (int argc, char **argv)
{
  SERVICE_TABLE_ENTRY dispatchTable[]=
  {
    {SZSERVICENAME, (LPSERVICE_MAIN_FUNCTION) service_main},
    {NULL, NULL}
  };

  if (StartServiceCtrlDispatcher (dispatchTable) == 0)
    AddToMessageLog ("StartServiceCtrlDispatcher failed.");
}

/* service_main, this routine performs the service initialization and then
   calls the user defined ServiceStart() routine to perform majority of the
   work. */

void WINAPI
service_main (DWORD dwArgc, LPSTR * lpszArgv)
{
  /* register our service control handler: */
  sshStatusHandle = RegisterServiceCtrlHandler (SZSERVICENAME, service_ctrl);
  if (sshStatusHandle == 0)
    return;

  ssStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  ssStatus.dwServiceSpecificExitCode = 0;

  /* report the status to the service control manager. */
  if (!ReportStatusToSCMgr (
			     SERVICE_START_PENDING,	/* service state */
			     NO_ERROR,	/* exit code */
			     3000))	/* wait hint */
    return;

  ServiceStart (dwArgc, lpszArgv);
}

/* service_ctrl, this function is called by the SCM whenever ControlService()
   is called on this service. */

void WINAPI
service_ctrl (DWORD dwCtrlCode)
{
  /* Handle the requested control code. */
  switch (dwCtrlCode)
    {
      case SERVICE_CONTROL_STOP:
      /* Stop the service. SERVICE_STOP_PENDING should be reported before
         setting the Stop Event - hServerStopEvent - in ServiceStop().  This
         avoids a race condition which may result in a 1053 - The Service did
         not respond... error. */
      ReportStatusToSCMgr (SERVICE_STOP_PENDING, NO_ERROR, 0);
      ServiceStop ();
      return;

    case SERVICE_CONTROL_SHUTDOWN:
      {
	DWORD os_error;
	int err;
	UnmountAllVolumes (NULL, &os_error, &err);
      }
      break;

    case SERVICE_CONTROL_INTERROGATE:
      /* Update the service status. */
      break;

    default:
      /* invalid control code */
      break;

    }

  ReportStatusToSCMgr (ssStatus.dwCurrentState, NO_ERROR, 0);
}

/* ReportStatusToSCMgr, sets the current status of the service and reports it
   to the Service Control Manager */

BOOL
ReportStatusToSCMgr (DWORD dwCurrentState,	/* the state of the service */
		     DWORD dwWin32ExitCode,	/* error code to report */
		     DWORD dwWaitHint)	/* worst case estimate to next
					   checkpoint */
{
  static DWORD dwCheckPoint = 1;
  BOOL fResult = TRUE;

  if (dwCurrentState == SERVICE_START_PENDING)
    ssStatus.dwControlsAccepted = 0;
  else
    ssStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

  ssStatus.dwCurrentState = dwCurrentState;
  ssStatus.dwWin32ExitCode = dwWin32ExitCode;
  ssStatus.dwWaitHint = dwWaitHint;

  if ((dwCurrentState == SERVICE_RUNNING) ||
      (dwCurrentState == SERVICE_STOPPED))
    ssStatus.dwCheckPoint = 0;
  else
    ssStatus.dwCheckPoint = dwCheckPoint++;

  /* Report the status of the service to the service control manager. */

  if (!(fResult = SetServiceStatus (sshStatusHandle, &ssStatus)))
    {
      AddToMessageLog ("SetServiceStatus");
    }

  return fResult;
}


/* AddToMessageLog, allows any thread to log an error message */

void
AddToMessageLog (LPSTR lpszMsg)
{
  char szMsg[256];
  HANDLE hEventSource;
  LPSTR lpszStrings[2];
  DWORD dwErr = GetLastError ();

  /* Use event logging to log the error */

  hEventSource = RegisterEventSource (NULL, SZSERVICENAME);

  sprintf (szMsg, "%s error: %d", SZSERVICENAME, dwErr);
  lpszStrings[0] = szMsg;
  lpszStrings[1] = lpszMsg;

  if (hEventSource != NULL)
    {
      ReportEvent (hEventSource,/* handle of event source */
		   EVENTLOG_ERROR_TYPE,	/* event type */
		   0,		/* event category */
		   0,		/* event ID */
		   NULL,	/* current user 's SID */
		   2,		/* strings in lpszStrings */
		   0,		/* no bytes of raw data */
		   lpszStrings,	/* array of error strings */
		   NULL);	/* no raw data */

      DeregisterEventSource (hEventSource);
    }
}

/* GetLastErrorText,  copies error message text to string */

LPSTR
GetLastErrorText (LPSTR lpszBuf, DWORD dwSize)
{
  DWORD dwRet;
  LPSTR lpszTemp = NULL;

  dwRet = FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ARGUMENT_ARRAY,
			 NULL,
			 GetLastError (),
			 LANG_NEUTRAL,
			 (LPSTR) & lpszTemp,
			 0,
			 NULL);

  /* supplied buffer is not long enough */
  if (dwRet == 0 || ((long) dwSize < (long) dwRet + 14))
    lpszBuf[0] = '\0';
  else
    {
      lpszTemp[lstrlen (lpszTemp) - 2] = '\0';	/* remove cr and newline
						   character */
      sprintf (lpszBuf, "%s (0x%x)", lpszTemp, GetLastError ());
    }

  if (lpszTemp != NULL)
    LocalFree ((HLOCAL) lpszTemp);

  return lpszBuf;
}

void
ServiceStart (DWORD dwArgc, LPTSTR * lpszArgv)
{
  DWORD dwWait, dwResult = ERROR_GEN_FAILURE;
  HANDLE hPipe = INVALID_HANDLE_VALUE;
  HANDLE hEvents[2] =
  {NULL, NULL};
  OVERLAPPED os;
  PSECURITY_DESCRIPTOR pSD = NULL;
  SECURITY_ATTRIBUTES sa;
  TCHAR szIn[80];
  TCHAR szOut[80];
  LPTSTR lpszPipeName = "\\\\.\\pipe\\e4mservice";
  BOOL bRet;
  DWORD cbRead;
  DWORD cbWritten;

  if (!ReportStatusToSCMgr (
			     SERVICE_START_PENDING,	/* service state */
			     NO_ERROR,	/* exit code */
			     3000))	/* wait hint */
    goto error;

  hServerStopEvent = CreateEvent (
				   NULL,	/* no security attributes */
				   TRUE,	/* manual reset event */
				   FALSE,	/* not-signalled */
				   NULL);	/* no name */

  if (hServerStopEvent == NULL)
    goto error;

  if (!ReportStatusToSCMgr (
			     SERVICE_START_PENDING,	/* service state */
			     NO_ERROR,	/* exit code */
			     3000))	/* wait hint */
    goto error;

  /* Try to open a handle to the device driver. It will be closed later. */
  hNTDriver = CreateFile (WIN32_ROOT_PREFIX, 0, 0, NULL, OPEN_EXISTING, 0, NULL);
  if (hNTDriver == INVALID_HANDLE_VALUE)
    {
      handleWin32Error (NULL);
      goto error;
    }

  if (!ReportStatusToSCMgr (
			     SERVICE_START_PENDING,	/* service state */
			     NO_ERROR,	/* exit code */
			     3000))	/* wait hint */
    goto error;

  /* create the event object object use in overlapped i/o */
  hEvents[1] = CreateEvent (
			     NULL,	/* no security attributes */
			     TRUE,	/* manual reset event */
			     FALSE,	/* not-signalled */
			     NULL);	/* no name */

  if (hEvents[1] == NULL)
    goto error;

  if (!ReportStatusToSCMgr (
			     SERVICE_START_PENDING,	/* service state */
			     NO_ERROR,	/* exit code */
			     3000))	/* wait hint */
    goto error;

  /* create a security descriptor that allows anyone to write to the pipe */
  pSD = (PSECURITY_DESCRIPTOR) malloc (SECURITY_DESCRIPTOR_MIN_LENGTH);

  if (pSD == NULL)
    goto error;

  if (InitializeSecurityDescriptor (pSD, SECURITY_DESCRIPTOR_REVISION) == 0)
    goto error;

  /* add a NULL disc.ACL to the security descriptor. */
  if (SetSecurityDescriptorDacl (pSD, TRUE, (PACL) NULL, FALSE) == 0)
    goto error;

  sa.nLength = sizeof (sa);
  sa.lpSecurityDescriptor = pSD;
  sa.bInheritHandle = TRUE;

  if (!ReportStatusToSCMgr (
			     SERVICE_START_PENDING,	/* service state */
			     NO_ERROR,	/* exit code */
			     3000))	/* wait hint */
    goto error;

  hPipe = CreateNamedPipe (
			    lpszPipeName,	/* name of pipe */
			    FILE_FLAG_OVERLAPPED |
			    PIPE_ACCESS_DUPLEX,	/* pipe open mode */
			    PIPE_TYPE_MESSAGE |
			    PIPE_READMODE_MESSAGE |
			    PIPE_WAIT,	/* pipe IO type */
			    1,	/* number of instances */
			    0,	/* size of outbuf (0 == allocate as
				   necessary) */
			    0,	/* size of inbuf */
			    1000,	/* default time-out value */
			    &sa);	/* security attributes */

  if (hPipe == INVALID_HANDLE_VALUE)
    {
      AddToMessageLog (TEXT ("Unable to create named pipe"));
      goto error;
    }

  hEvents[0] = hServerStopEvent;

  if (!ReportStatusToSCMgr (
			     SERVICE_RUNNING,	/* service state */
			     NO_ERROR,	/* exit code */
			     0))/* wait hint */
    goto error;

  for (;;)
    {
      int nDosDriveNo = -1, err;
      DWORD os_error;

      /* init the overlapped structure */
      memset (&os, 0, sizeof (OVERLAPPED));
      os.hEvent = hEvents[1];
      ResetEvent (hEvents[1]);

      /* wait for a connection... */
      ConnectNamedPipe (hPipe, &os);

      if (GetLastError ()== ERROR_IO_PENDING)
	{
	  dwWait = WaitForMultipleObjects (2, hEvents, FALSE, INFINITE);
	  if (dwWait != WAIT_OBJECT_0 + 1)	/* not overlapped i/o event -
						   error occurred, */
	    break;		/* or server stop signaled */
	}

      /* init the overlapped structure */
      memset (&os, 0, sizeof (OVERLAPPED));
      os.hEvent = hEvents[1];
      ResetEvent (hEvents[1]);

      /* grab whatever's coming through the pipe... */
      bRet = ReadFile (
			hPipe,	/* file to read from */
			szIn,	/* address of input buffer */
			sizeof (szIn),	/* number of bytes to read */
			&cbRead,/* number of bytes read */
			&os);	/* overlapped stuff, not needed */

      if (bRet == FALSE && (GetLastError ()== ERROR_IO_PENDING))
	{
	  dwWait = WaitForMultipleObjects (2, hEvents, FALSE, INFINITE);
	  if (dwWait != WAIT_OBJECT_0 + 1)	/* not overlapped i/o event -
						   error occurred, */
	    break;		/* or server stop signaled */
	}

      sscanf (szIn, "%s %d", szOut, &nDosDriveNo);

      if (strcmp (szOut, "unmount") == 0)
	{
	  if (UnmountVolume (NULL, nDosDriveNo, &os_error, &err) == FALSE)
	    {
	      sprintf (szOut, "-ERR %d %lu", err, os_error);
	    }
	  else
	    {
	      sprintf (szOut, "+OK unmounted");
	    }
	}
      else
	{
	  sprintf (szOut, "-ERR invalid command");
	}

      /* init the overlapped structure */
      memset (&os, 0, sizeof (OVERLAPPED));
      os.hEvent = hEvents[1];
      ResetEvent (hEvents[1]);

      /* send it back out... */
      bRet = WriteFile (
			 hPipe,	/* file to write to */
			 szOut,	/* address of output buffer */
			 sizeof (szOut),	/* number of bytes to write */
			 &cbWritten,	/* number of bytes written */
			 &os);	/* overlapped stuff, not needed */

      if (bRet == FALSE && (GetLastError ()== ERROR_IO_PENDING))
	{
	  dwWait = WaitForMultipleObjects (2, hEvents, FALSE, INFINITE);
	  if (dwWait != WAIT_OBJECT_0 + 1)	/* not overlapped i/o event -
						   error occurred, */
	    break;		/* or server stop signaled */
	}

      /* drop the connection... */
      DisconnectNamedPipe (hPipe);
    }


  dwResult = NO_ERROR;

error:
  if (hServerStopEvent != NULL)
    CloseHandle (hServerStopEvent);

  if (hEvents[1] != NULL)
    CloseHandle (hEvents[1]);

  if (hNTDriver != INVALID_HANDLE_VALUE)
    CloseHandle (hNTDriver);

  if (hPipe != INVALID_HANDLE_VALUE)
    CloseHandle (hPipe);

  if (pSD != NULL)
    free (pSD);

  ReportStatusToSCMgr (SERVICE_STOPPED, dwResult, 0);
}

void
ServiceStop ()
{
  if (hServerStopEvent != NULL)
    SetEvent (hServerStopEvent);
}

void
handleWin32Error (HWND dummy)
{
  char tmp[256] =
  {0};

  if (dummy);			/* Remove warning */

  GetLastErrorText (tmp, sizeof (tmp));

  AddToMessageLog (tmp);
}
