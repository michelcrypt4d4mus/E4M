/* Copyright (C) 1998-99 Paul Le Roux. All rights reserved. Please see the
   file license.txt for full license details. paulca@rocketmail.com */

#define HAS_ARGUMENT	1
#define HAS_NO_ARGUMENT !HAS_ARGUMENT

typedef struct argument_t
{
	char long_name[16];
	char short_name[8];
} argument;

typedef struct argumentspec_t
{
	argument *args;
	int		 arg_cnt;
} argumentspec;

/* Everything below this line is automatically updated by the -mkproto-tool- */

BOOL WINAPI CommandHelpDlgProc ( HWND hwndDlg , UINT msg , WPARAM wParam , LPARAM lParam );
int Win32CommandLine ( char *lpszCommandLine , char ***lpszArgs );
int GetArgSepPosOffset ( char *lpszArgument );
int GetArgumentID ( argumentspec *as , char *lpszArgument , int *nArgPos );
int GetArgumentValue ( char **lpszCommandLineArgs , int nArgPos , int *nArgIdx , int nNoCommandLineArgs , char *lpszValue , int nValueSize );
