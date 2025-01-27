/* Everything below this line is automatically updated by the -mkproto-tool- */

BOOL StatDeleteFile ( char *lpszFile );
BOOL StatRemoveDirectory ( char *lpszDir );
HRESULT CreateLink ( char *lpszPathObj , char *lpszArguments , char *lpszPathLink );
void GetProgramPath ( HWND hwndDlg , char *path );
void StatusMessage ( HWND hwndDlg , char *head , char *txt );
void RegMessage ( HWND hwndDlg , char *txt );
void CopyMessage ( HWND hwndDlg , char *txt );
void RemoveMessage ( HWND hwndDlg , char *txt );
void ServiceMessage ( HWND hwndDlg , char *txt );
void IconMessage ( HWND hwndDlg , char *txt );
int CALLBACK BrowseCallbackProc ( HWND hwnd , UINT uMsg , LPARAM lp , LPARAM pData );
BOOL BrowseFiles2 ( HWND hwndDlg , char *lpszTitle , char *lpszFileName );
void LoadLicense ( HWND hwndDlg );
BOOL DoFilesInstall ( HWND hwndDlg , char *szDestDir , BOOL bUninstallSupport );
BOOL DoRegInstall ( HWND hwndDlg , char *szDestDir , BOOL bInstallType , BOOL bUninstallSupport );
BOOL DoRegUninstall ( HWND hwndDlg );
BOOL DoServiceUninstall ( HWND hwndDlg , char *lpszService );
BOOL DoDriverUnload ( HWND hwndDlg );
BOOL DoServiceInstall ( HWND hwndDlg );
BOOL DoDriverInstall ( HWND hwndDlg );
BOOL DoShortcutsInstall ( HWND hwndDlg , char *szDestDir , BOOL bProgGroup );
void RebootPrompt ( HWND hwndDlg , BOOL bOK );
void DoUninstall ( void *hwndDlg );
void DoInstall ( void *hwndDlg );
BOOL IsAdmin ( void );
BOOL WINAPI InstallDlgProc ( HWND hwndDlg , UINT msg , WPARAM wParam , LPARAM lParam );
int WINAPI WINMAIN ( HINSTANCE hInstance , HINSTANCE hPrevInstance , char *lpszCommandLine , int nCmdShow );
