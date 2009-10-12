#include <wctype.h>
#include <windows.h>
#include <shlobj.h>

#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#ifndef countof
#  define countof(array) (sizeof (array) / sizeof (*(array)))
#endif

#define SE_SHUTDOWN_NAMEA   "SeShutdownPrivilege"

#ifndef WC_NO_BEST_FIT_CHARS
#  define WC_NO_BEST_FIT_CHARS 0x00000400
#endif

#define GETPROC(fn) pfn##fn = (PFN##fn)GetProcAddress(module, #fn)

typedef BOOL (WINAPI *PFNSHGetSpecialFolderPathA)(HWND, char*, int, BOOL);
typedef BOOL (WINAPI *PFNSHGetSpecialFolderPathW)(HWND, WCHAR*, int, BOOL);
typedef HRESULT (WINAPI *PFNSHGetFolderPathA)(HWND, int, HANDLE, DWORD, LPTSTR);
typedef HRESULT (WINAPI *PFNSHGetFolderPathW)(HWND, int, HANDLE, DWORD, LPWSTR);
typedef BOOL (WINAPI *PFNCreateEnvironmentBlock)(void**, HANDLE, BOOL);
typedef BOOL (WINAPI *PFNDestroyEnvironmentBlock)(void*);
typedef int (__stdcall *PFNDllRegisterServer)(void);
typedef int (__stdcall *PFNDllUnregisterServer)(void);
typedef DWORD (__stdcall *PFNNetApiBufferFree)(void*);
typedef DWORD (__stdcall *PFNNetWkstaGetInfo)(LPWSTR, DWORD, void*);

typedef BOOL (__stdcall *PFNOpenProcessToken)(HANDLE, DWORD, HANDLE*);
typedef BOOL (__stdcall *PFNOpenThreadToken)(HANDLE, DWORD, BOOL, HANDLE*);
typedef BOOL (__stdcall *PFNGetTokenInformation)(HANDLE, TOKEN_INFORMATION_CLASS, void*, DWORD, DWORD*);
typedef BOOL (__stdcall *PFNAllocateAndInitializeSid)(PSID_IDENTIFIER_AUTHORITY, BYTE, DWORD, DWORD,
                                                      DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, PSID*);
typedef BOOL (__stdcall *PFNEqualSid)(PSID, PSID);
typedef void* (__stdcall *PFNFreeSid)(PSID);
typedef BOOL (__stdcall *PFNIsUserAnAdmin)(void);

#ifndef CSIDL_MYMUSIC
#   define CSIDL_MYMUSIC              0x000D
#endif
#ifndef CSIDL_MYVIDEO
#   define CSIDL_MYVIDEO              0x000E
#endif
#ifndef CSIDL_LOCAL_APPDATA
#   define CSIDL_LOCAL_APPDATA        0x001C
#endif
#ifndef CSIDL_COMMON_FAVORITES
#   define CSIDL_COMMON_FAVORITES     0x001F
#endif
#ifndef CSIDL_INTERNET_CACHE
#   define CSIDL_INTERNET_CACHE       0x0020
#endif
#ifndef CSIDL_COOKIES
#   define CSIDL_COOKIES              0x0021
#endif
#ifndef CSIDL_HISTORY
#   define CSIDL_HISTORY              0x0022
#endif
#ifndef CSIDL_COMMON_APPDATA
#   define CSIDL_COMMON_APPDATA       0x0023
#endif
#ifndef CSIDL_WINDOWS
#   define CSIDL_WINDOWS              0x0024
#endif
#ifndef CSIDL_PROGRAM_FILES
#   define CSIDL_PROGRAM_FILES        0x0026
#endif
#ifndef CSIDL_MYPICTURES
#   define CSIDL_MYPICTURES           0x0027
#endif
#ifndef CSIDL_PROFILE
#   define CSIDL_PROFILE              0x0028
#endif
#ifndef CSIDL_PROGRAM_FILES_COMMON
#   define CSIDL_PROGRAM_FILES_COMMON 0x002B
#endif
#ifndef CSIDL_COMMON_TEMPLATES
#   define CSIDL_COMMON_TEMPLATES     0x002D
#endif
#ifndef CSIDL_COMMON_DOCUMENTS
#   define CSIDL_COMMON_DOCUMENTS     0x002E
#endif
#ifndef CSIDL_COMMON_ADMINTOOLS
#   define CSIDL_COMMON_ADMINTOOLS    0x002F
#endif
#ifndef CSIDL_ADMINTOOLS
#   define CSIDL_ADMINTOOLS           0x0030
#endif
#ifndef CSIDL_COMMON_MUSIC
#   define CSIDL_COMMON_MUSIC         0x0035
#endif
#ifndef CSIDL_COMMON_PICTURES
#   define CSIDL_COMMON_PICTURES      0x0036
#endif
#ifndef CSIDL_COMMON_VIDEO
#   define CSIDL_COMMON_VIDEO         0x0037
#endif
#ifndef CSIDL_CDBURN_AREA
#   define CSIDL_CDBURN_AREA          0x003B
#endif
#ifndef CSIDL_FLAG_CREATE
#   define CSIDL_FLAG_CREATE          0x8000
#endif

/* Use explicit struct definition because wSuiteMask and
 * wProductType are not defined in the VC++ 6.0 headers.
 * WORD type has been replaced by unsigned short because
 * WORD is already used by Perl itself.
 */
struct {
    DWORD dwOSVersionInfoSize;
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    DWORD dwPlatformId;
    CHAR  szCSDVersion[128];
    unsigned short wServicePackMajor;
    unsigned short wServicePackMinor;
    unsigned short wSuiteMask;
    BYTE  wProductType;
    BYTE  wReserved;
}   g_osver = {0, 0, 0, 0, 0, "", 0, 0, 0, 0, 0};
BOOL g_osver_ex = TRUE;

#define ONE_K_BUFSIZE	1024

int
IsWin95(void)
{
    return (g_osver.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS);
}

int
IsWinNT(void)
{
    return (g_osver.dwPlatformId == VER_PLATFORM_WIN32_NT);
}

int
IsWin2000(void)
{
    return (g_osver.dwMajorVersion > 4);
}

/* Convert SV to wide character string.  The return value must be
 * freed using Safefree().
 */
WCHAR*
sv_to_wstr(pTHX_ SV *sv)
{
    DWORD wlen;
    WCHAR *wstr;
    STRLEN len;
    char *str = SvPV(sv, len);
    UINT cp = SvUTF8(sv) ? CP_UTF8 : CP_ACP;

    wlen = MultiByteToWideChar(cp, 0, str, (int)(len+1), NULL, 0);
    New(0, wstr, wlen, WCHAR);
    MultiByteToWideChar(cp, 0, str, (int)(len+1), wstr, wlen);

    return wstr;
}

/* Convert wide character string to mortal SV.  Use UTF8 encoding
 * if the string cannot be represented in the system codepage.
 */
SV *
wstr_to_sv(pTHX_ WCHAR *wstr)
{
    int wlen = (int)wcslen(wstr)+1;
    BOOL use_default = FALSE;
    int len = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, wstr, wlen, NULL, 0, NULL, NULL);
    SV *sv = sv_2mortal(newSV(len));

    len = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, wstr, wlen, SvPVX(sv), len, NULL, &use_default);
    if (use_default) {
        len = WideCharToMultiByte(CP_UTF8, 0, wstr, wlen, NULL, 0, NULL, NULL);
        sv_grow(sv, len);
        len = WideCharToMultiByte(CP_UTF8, 0, wstr, wlen, SvPVX(sv), len, NULL, NULL);
        SvUTF8_on(sv);
    }
    /* Shouldn't really ever fail since we ask for the required length first, but who knows... */
    if (len) {
        SvPOK_on(sv);
        SvCUR_set(sv, len-1);
    }
    return sv;
}

/* Retrieve a variable from the Unicode environment in a mortal SV.
 *
 * Recreates the Unicode environment because a bug in earlier Perl versions
 * overwrites it with the ANSI version, which contains replacement
 * characters for the characters not in the ANSI codepage.
 */
SV*
get_unicode_env(pTHX_ WCHAR *name)
{
    SV *sv = NULL;
    void *env;
    HANDLE token;
    HMODULE module;
    PFNOpenProcessToken pfnOpenProcessToken;

    /* Get security token for the current process owner */
    module = LoadLibrary("advapi32.dll");
    if (!module)
        return NULL;

    GETPROC(OpenProcessToken);

    if (pfnOpenProcessToken == NULL ||
        !pfnOpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &token))
    {
        FreeLibrary(module);
        return NULL;
    }
    FreeLibrary(module);

    /* Create a Unicode environment block for this process */
    module = LoadLibrary("userenv.dll");
    if (module) {
        PFNCreateEnvironmentBlock pfnCreateEnvironmentBlock;
        PFNDestroyEnvironmentBlock pfnDestroyEnvironmentBlock;

        GETPROC(CreateEnvironmentBlock);
        GETPROC(DestroyEnvironmentBlock);

        if (pfnCreateEnvironmentBlock && pfnDestroyEnvironmentBlock &&
            pfnCreateEnvironmentBlock(&env, token, FALSE))
        {
            size_t name_len = wcslen(name);
            WCHAR *entry = env;
            while (*entry) {
                size_t i;
                size_t entry_len = wcslen(entry);
                BOOL equal = (entry_len > name_len) && (entry[name_len] == '=');

                for (i=0; equal && i < name_len; ++i)
                    equal = (towupper(entry[i]) == towupper(name[i]));

                if (equal) {
                    sv = wstr_to_sv(aTHX_ entry+name_len+1);
                    break;
                }
                entry += entry_len+1;
            }
            pfnDestroyEnvironmentBlock(env);
        }
        FreeLibrary(module);
    }
    CloseHandle(token);
    return sv;
}

/* Define both an ANSI and a Wide version of win32_longpath */

#define CHAR_T            char
#define WIN32_FIND_DATA_T WIN32_FIND_DATAA
#define FN_FINDFIRSTFILE  FindFirstFileA
#define FN_STRLEN         strlen
#define FN_STRCPY         strcpy
#define LONGPATH          my_longpathA
#include "longpath.inc"

#define CHAR_T            WCHAR
#define WIN32_FIND_DATA_T WIN32_FIND_DATAW
#define FN_FINDFIRSTFILE  FindFirstFileW
#define FN_STRLEN         wcslen
#define FN_STRCPY         wcscpy
#define LONGPATH          my_longpathW
#include "longpath.inc"

/* The my_ansipath() function takes a Unicode filename and converts it
 * into the current Windows codepage. If some characters cannot be mapped,
 * then it will convert the short name instead.
 *
 * The buffer to the ansi pathname must be freed with Safefree() when it
 * it no longer needed.
 *
 * The argument to my_ansipath() must exist before this function is
 * called; otherwise there is no way to determine the short path name.
 *
 * Ideas for future refinement:
 * - Only convert those segments of the path that are not in the current
 *   codepage, but leave the other segments in their long form.
 * - If the resulting name is longer than MAX_PATH, start converting
 *   additional path segments into short names until the full name
 *   is shorter than MAX_PATH.  Shorten the filename part last!
 */

/* This is a modified version of core Perl win32/win32.c(win32_ansipath).
 * It uses New() etc. instead of win32_malloc().
 */

char *
my_ansipath(const WCHAR *widename)
{
    char *name;
    BOOL use_default = FALSE;
    int widelen = (int)wcslen(widename)+1;
    int len = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, widename, widelen,
                                  NULL, 0, NULL, NULL);
    New(0, name, len, char);
    WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, widename, widelen,
                        name, len, NULL, &use_default);
    if (use_default) {
        DWORD shortlen = GetShortPathNameW(widename, NULL, 0);
        if (shortlen) {
            WCHAR *shortname;
            New(0, shortname, shortlen, WCHAR);
            shortlen = GetShortPathNameW(widename, shortname, shortlen)+1;

            len = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, shortname, shortlen,
                                      NULL, 0, NULL, NULL);
            Renew(name, len, char);
            WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, shortname, shortlen,
                                name, len, NULL, NULL);
            Safefree(shortname);
        }
    }
    return name;
}

/* Convert wide character path to ANSI path and return as mortal SV. */
SV*
wstr_to_ansipath(pTHX_ WCHAR *wstr)
{
    char *ansi = my_ansipath(wstr);
    SV *sv = sv_2mortal(newSVpvn(ansi, strlen(ansi)));
    Safefree(ansi);
    return sv;
}

#ifdef __CYGWIN__

char*
get_childdir(void)
{
    dTHX;
    char* ptr;

    if (IsWin2000()) {
        WCHAR filename[MAX_PATH+1];
        GetCurrentDirectoryW(MAX_PATH+1, filename);
        ptr = my_ansipath(filename);
    }
    else {
        char filename[MAX_PATH+1];
        GetCurrentDirectoryA(MAX_PATH+1, filename);
        New(0, ptr, strlen(filename)+1, char);
        strcpy(ptr, filename);
    }
    return ptr;
}

void
free_childdir(char *d)
{
    dTHX;
    Safefree(d);
}

void*
get_childenv(void)
{
    return NULL;
}

void
free_childenv(void *d)
{
}

#  define PerlDir_mapA(dir) (dir)

#endif

XS(w32_ExpandEnvironmentStrings)
{
    dXSARGS;

    if (items != 1)
	croak("usage: Win32::ExpandEnvironmentStrings($String);\n");

    if (IsWin2000()) {
        WCHAR value[31*1024];
        WCHAR *source = sv_to_wstr(aTHX_ ST(0));
        ExpandEnvironmentStringsW(source, value, countof(value)-1);
        ST(0) = wstr_to_sv(aTHX_ value);
        Safefree(source);
        XSRETURN(1);
    }
    else {
        char value[31*1024];
        ExpandEnvironmentStringsA(SvPV_nolen(ST(0)), value, countof(value)-2);
        XSRETURN_PV(value);
    }
}

XS(w32_IsAdminUser)
{
    dXSARGS;
    HMODULE                     module;
    PFNIsUserAnAdmin            pfnIsUserAnAdmin;
    PFNOpenThreadToken          pfnOpenThreadToken;
    PFNOpenProcessToken         pfnOpenProcessToken;
    PFNGetTokenInformation      pfnGetTokenInformation;
    PFNAllocateAndInitializeSid pfnAllocateAndInitializeSid;
    PFNEqualSid                 pfnEqualSid;
    PFNFreeSid                  pfnFreeSid;
    HANDLE                      hTok;
    DWORD                       dwTokInfoLen;
    TOKEN_GROUPS                *lpTokInfo;
    SID_IDENTIFIER_AUTHORITY    NtAuth = SECURITY_NT_AUTHORITY;
    PSID                        pAdminSid;
    int                         iRetVal;
    unsigned int                i;

    if (items)
        croak("usage: Win32::IsAdminUser()");

    /* There is no concept of "Administrator" user accounts on Win9x systems,
       so just return true. */
    if (IsWin95())
        XSRETURN_YES;

    /* Use IsUserAnAdmin() when available.  On Vista this will only return TRUE
     * if the process is running with elevated privileges and not just when the
     * process owner is a member of the "Administrators" group.
     */
    module = LoadLibrary("shell32.dll");
    if (module) {
        GETPROC(IsUserAnAdmin);
        if (pfnIsUserAnAdmin) {
            EXTEND(SP, 1);
            ST(0) = sv_2mortal(newSViv(pfnIsUserAnAdmin() ? 1 : 0));
            FreeLibrary(module);
            XSRETURN(1);
        }
        FreeLibrary(module);
    }

    module = LoadLibrary("advapi32.dll");
    if (!module) {
        warn("Cannot load advapi32.dll library");
        XSRETURN_UNDEF;
    }

    GETPROC(OpenThreadToken);
    GETPROC(OpenProcessToken);
    GETPROC(GetTokenInformation);
    GETPROC(AllocateAndInitializeSid);
    GETPROC(EqualSid);
    GETPROC(FreeSid);

    if (!(pfnOpenThreadToken && pfnOpenProcessToken &&
          pfnGetTokenInformation && pfnAllocateAndInitializeSid &&
          pfnEqualSid && pfnFreeSid))
    {
        warn("Cannot load functions from advapi32.dll library");
        FreeLibrary(module);
        XSRETURN_UNDEF;
    }

    if (!pfnOpenThreadToken(GetCurrentThread(), TOKEN_QUERY, FALSE, &hTok)) {
        if (!pfnOpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hTok)) {
            warn("Cannot open thread token or process token");
            FreeLibrary(module);
            XSRETURN_UNDEF;
        }
    }

    pfnGetTokenInformation(hTok, TokenGroups, NULL, 0, &dwTokInfoLen);
    if (!New(1, lpTokInfo, dwTokInfoLen, TOKEN_GROUPS)) {
        warn("Cannot allocate token information structure");
        CloseHandle(hTok);
        FreeLibrary(module);
        XSRETURN_UNDEF;
    }

    if (!pfnGetTokenInformation(hTok, TokenGroups, lpTokInfo, dwTokInfoLen,
            &dwTokInfoLen))
    {
        warn("Cannot get token information");
        Safefree(lpTokInfo);
        CloseHandle(hTok);
        FreeLibrary(module);
        XSRETURN_UNDEF;
    }

    if (!pfnAllocateAndInitializeSid(&NtAuth, 2, SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pAdminSid))
    {
        warn("Cannot allocate administrators' SID");
        Safefree(lpTokInfo);
        CloseHandle(hTok);
        FreeLibrary(module);
        XSRETURN_UNDEF;
    }

    iRetVal = 0;
    for (i = 0; i < lpTokInfo->GroupCount; ++i) {
        if (pfnEqualSid(lpTokInfo->Groups[i].Sid, pAdminSid)) {
            iRetVal = 1;
            break;
        }
    }

    pfnFreeSid(pAdminSid);
    Safefree(lpTokInfo);
    CloseHandle(hTok);
    FreeLibrary(module);

    EXTEND(SP, 1);
    ST(0) = sv_2mortal(newSViv(iRetVal));
    XSRETURN(1);
}

XS(w32_LookupAccountName)
{
    dXSARGS;
    char SID[400];
    DWORD SIDLen;
    SID_NAME_USE snu;
    char Domain[256];
    DWORD DomLen;
    BOOL bResult;

    if (items != 5)
	croak("usage: Win32::LookupAccountName($system, $account, $domain, "
	      "$sid, $sidtype);\n");

    SIDLen = sizeof(SID);
    DomLen = sizeof(Domain);

    bResult = LookupAccountNameA(SvPV_nolen(ST(0)),	/* System */
                                 SvPV_nolen(ST(1)),	/* Account name */
                                 &SID,			/* SID structure */
                                 &SIDLen,		/* Size of SID buffer */
                                 Domain,		/* Domain buffer */
                                 &DomLen,		/* Domain buffer size */
                                 &snu);			/* SID name type */
    if (bResult) {
	sv_setpv(ST(2), Domain);
	sv_setpvn(ST(3), SID, SIDLen);
	sv_setiv(ST(4), snu);
	XSRETURN_YES;
    }
    XSRETURN_NO;
}


XS(w32_LookupAccountSID)
{
    dXSARGS;
    PSID sid;
    char Account[256];
    DWORD AcctLen = sizeof(Account);
    char Domain[256];
    DWORD DomLen = sizeof(Domain);
    SID_NAME_USE snu;
    BOOL bResult;

    if (items != 5)
	croak("usage: Win32::LookupAccountSID($system, $sid, $account, $domain, $sidtype);\n");

    sid = SvPV_nolen(ST(1));
    if (IsValidSid(sid)) {
        bResult = LookupAccountSidA(SvPV_nolen(ST(0)),	/* System */
                                    sid,		/* SID structure */
                                    Account,		/* Account name buffer */
                                    &AcctLen,		/* name buffer length */
                                    Domain,		/* Domain buffer */
                                    &DomLen,		/* Domain buffer length */
                                    &snu);		/* SID name type */
	if (bResult) {
	    sv_setpv(ST(2), Account);
	    sv_setpv(ST(3), Domain);
	    sv_setiv(ST(4), (IV)snu);
	    XSRETURN_YES;
	}
    }
    XSRETURN_NO;
}

XS(w32_InitiateSystemShutdown)
{
    dXSARGS;
    HANDLE hToken;              /* handle to process token   */
    TOKEN_PRIVILEGES tkp;       /* pointer to token structure  */
    BOOL bRet;
    char *machineName, *message;

    if (items != 5)
	croak("usage: Win32::InitiateSystemShutdown($machineName, $message, "
	      "$timeOut, $forceClose, $reboot);\n");

    machineName = SvPV_nolen(ST(0));

    if (OpenProcessToken(GetCurrentProcess(),
			 TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
			 &hToken))
    {
        LookupPrivilegeValueA(machineName,
                              SE_SHUTDOWN_NAMEA,
                              &tkp.Privileges[0].Luid);

	tkp.PrivilegeCount = 1; /* only setting one */
	tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	/* Get shutdown privilege for this process. */
	AdjustTokenPrivileges(hToken, FALSE, &tkp, 0,
			      (PTOKEN_PRIVILEGES)NULL, 0);
    }

    message = SvPV_nolen(ST(1));
    bRet = InitiateSystemShutdownA(machineName, message, (DWORD)SvIV(ST(2)),
                                   (BOOL)SvIV(ST(3)), (BOOL)SvIV(ST(4)));

    /* Disable shutdown privilege. */
    tkp.Privileges[0].Attributes = 0; 
    AdjustTokenPrivileges(hToken, FALSE, &tkp, 0,
			  (PTOKEN_PRIVILEGES)NULL, 0); 
    CloseHandle(hToken);
    XSRETURN_IV(bRet);
}

XS(w32_AbortSystemShutdown)
{
    dXSARGS;
    HANDLE hToken;              /* handle to process token   */
    TOKEN_PRIVILEGES tkp;       /* pointer to token structure  */
    BOOL bRet;
    char *machineName;

    if (items != 1)
	croak("usage: Win32::AbortSystemShutdown($machineName);\n");

    machineName = SvPV_nolen(ST(0));

    if (OpenProcessToken(GetCurrentProcess(),
			 TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
			 &hToken))
    {
        LookupPrivilegeValueA(machineName,
                              SE_SHUTDOWN_NAMEA,
                              &tkp.Privileges[0].Luid);

	tkp.PrivilegeCount = 1; /* only setting one */
	tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	/* Get shutdown privilege for this process. */
	AdjustTokenPrivileges(hToken, FALSE, &tkp, 0,
			      (PTOKEN_PRIVILEGES)NULL, 0);
    }

    bRet = AbortSystemShutdownA(machineName);

    /* Disable shutdown privilege. */
    tkp.Privileges[0].Attributes = 0;
    AdjustTokenPrivileges(hToken, FALSE, &tkp, 0,
			  (PTOKEN_PRIVILEGES)NULL, 0);
    CloseHandle(hToken);
    XSRETURN_IV(bRet);
}


XS(w32_MsgBox)
{
    dXSARGS;
    DWORD flags = MB_ICONEXCLAMATION;
    I32 result;

    if (items < 1 || items > 3)
	croak("usage: Win32::MsgBox($message [, $flags [, $title]]);\n");

    if (items > 1)
        flags = (DWORD)SvIV(ST(1));

    if (IsWin2000()) {
        WCHAR *title = NULL;
        WCHAR *msg = sv_to_wstr(aTHX_ ST(0));
        if (items > 2)
            title = sv_to_wstr(aTHX_ ST(2));
        result = MessageBoxW(GetActiveWindow(), msg, title ? title : L"Perl", flags);
        Safefree(msg);
        if (title)
            Safefree(title);
    }
    else {
        char *title = "Perl";
        char *msg = SvPV_nolen(ST(0));
        if (items > 2)
            title = SvPV_nolen(ST(2));
        result = MessageBoxA(GetActiveWindow(), msg, title, flags);
    }
    XSRETURN_IV(result);
}

XS(w32_LoadLibrary)
{
    dXSARGS;
    HANDLE hHandle;

    if (items != 1)
	croak("usage: Win32::LoadLibrary($libname)\n");
    hHandle = LoadLibraryA(SvPV_nolen(ST(0)));
#ifdef _WIN64
    XSRETURN_IV((DWORD_PTR)hHandle);
#else
    XSRETURN_IV((DWORD)hHandle);
#endif
}

XS(w32_FreeLibrary)
{
    dXSARGS;

    if (items != 1)
	croak("usage: Win32::FreeLibrary($handle)\n");
    if (FreeLibrary(INT2PTR(HINSTANCE, SvIV(ST(0))))) {
	XSRETURN_YES;
    }
    XSRETURN_NO;
}

XS(w32_GetProcAddress)
{
    dXSARGS;

    if (items != 2)
	croak("usage: Win32::GetProcAddress($hinstance, $procname)\n");
    XSRETURN_IV(PTR2IV(GetProcAddress(INT2PTR(HINSTANCE, SvIV(ST(0))), SvPV_nolen(ST(1)))));
}

XS(w32_RegisterServer)
{
    dXSARGS;
    BOOL result = FALSE;
    HMODULE module;

    if (items != 1)
	croak("usage: Win32::RegisterServer($libname)\n");

    module = LoadLibraryA(SvPV_nolen(ST(0)));
    if (module) {
	PFNDllRegisterServer pfnDllRegisterServer;
        GETPROC(DllRegisterServer);
	if (pfnDllRegisterServer && pfnDllRegisterServer() == 0)
	    result = TRUE;
	FreeLibrary(module);
    }
    ST(0) = boolSV(result);
    XSRETURN(1);
}

XS(w32_UnregisterServer)
{
    dXSARGS;
    BOOL result = FALSE;
    HINSTANCE module;

    if (items != 1)
	croak("usage: Win32::UnregisterServer($libname)\n");

    module = LoadLibraryA(SvPV_nolen(ST(0)));
    if (module) {
	PFNDllUnregisterServer pfnDllUnregisterServer;
        GETPROC(DllUnregisterServer);
	if (pfnDllUnregisterServer && pfnDllUnregisterServer() == 0)
	    result = TRUE;
	FreeLibrary(module);
    }
    ST(0) = boolSV(result);
    XSRETURN(1);
}

/* XXX rather bogus */
XS(w32_GetArchName)
{
    dXSARGS;
    XSRETURN_PV(getenv("PROCESSOR_ARCHITECTURE"));
}

XS(w32_GetChipName)
{
    dXSARGS;
    SYSTEM_INFO sysinfo;

    Zero(&sysinfo,1,SYSTEM_INFO);
    GetSystemInfo(&sysinfo);
    /* XXX docs say dwProcessorType is deprecated on NT */
    XSRETURN_IV(sysinfo.dwProcessorType);
}

XS(w32_GuidGen)
{
    dXSARGS;
    GUID guid;
    char szGUID[50] = {'\0'};
    HRESULT  hr     = CoCreateGuid(&guid);

    if (SUCCEEDED(hr)) {
	LPOLESTR pStr = NULL;
	if (SUCCEEDED(StringFromCLSID(&guid, &pStr))) {
            WideCharToMultiByte(CP_ACP, 0, pStr, (int)wcslen(pStr), szGUID,
                                sizeof(szGUID), NULL, NULL);
            CoTaskMemFree(pStr);
            XSRETURN_PV(szGUID);
        }
    }
    XSRETURN_UNDEF;
}

XS(w32_GetFolderPath)
{
    dXSARGS;
    char path[MAX_PATH+1];
    WCHAR wpath[MAX_PATH+1];
    int folder;
    int create = 0;
    HMODULE module;

    if (items != 1 && items != 2)
	croak("usage: Win32::GetFolderPath($csidl [, $create])\n");

    folder = (int)SvIV(ST(0));
    if (items == 2)
        create = SvTRUE(ST(1)) ? CSIDL_FLAG_CREATE : 0;

    module = LoadLibrary("shfolder.dll");
    if (module) {
        PFNSHGetFolderPathA pfna;
        if (IsWin2000()) {
            PFNSHGetFolderPathW pfnw;
            pfnw = (PFNSHGetFolderPathW)GetProcAddress(module, "SHGetFolderPathW");
            if (pfnw && SUCCEEDED(pfnw(NULL, folder|create, NULL, 0, wpath))) {
                FreeLibrary(module);
                ST(0) = wstr_to_ansipath(aTHX_ wpath);
                XSRETURN(1);
            }
        }
        pfna = (PFNSHGetFolderPathA)GetProcAddress(module, "SHGetFolderPathA");
        if (pfna && SUCCEEDED(pfna(NULL, folder|create, NULL, 0, path))) {
            FreeLibrary(module);
            XSRETURN_PV(path);
        }
        FreeLibrary(module);
    }

    module = LoadLibrary("shell32.dll");
    if (module) {
        PFNSHGetSpecialFolderPathA pfna;
        if (IsWin2000()) {
            PFNSHGetSpecialFolderPathW pfnw;
            pfnw = (PFNSHGetSpecialFolderPathW)GetProcAddress(module, "SHGetSpecialFolderPathW");
            if (pfnw && pfnw(NULL, wpath, folder, !!create)) {
                FreeLibrary(module);
                ST(0) = wstr_to_ansipath(aTHX_ wpath);
                XSRETURN(1);
            }
        }
        pfna = (PFNSHGetSpecialFolderPathA)GetProcAddress(module, "SHGetSpecialFolderPathA");
        if (pfna && pfna(NULL, path, folder, !!create)) {
            FreeLibrary(module);
            XSRETURN_PV(path);
        }
        FreeLibrary(module);
    }

    /* SHGetFolderPathW() and SHGetSpecialFolderPathW() may fail on older
     * Perl versions that have replaced the Unicode environment with an
     * ANSI version.  Let's go spelunking in the registry now...
     */
    if (IsWin2000()) {
        SV *sv;
        HKEY hkey;
        HKEY root = HKEY_CURRENT_USER;
        WCHAR *name = NULL;

        switch (folder) {
        case CSIDL_ADMINTOOLS:                  name = L"Administrative Tools";        break;
        case CSIDL_APPDATA:                     name = L"AppData";                     break;
        case CSIDL_CDBURN_AREA:                 name = L"CD Burning";                  break;
        case CSIDL_COOKIES:                     name = L"Cookies";                     break;
        case CSIDL_DESKTOP:
        case CSIDL_DESKTOPDIRECTORY:            name = L"Desktop";                     break;
        case CSIDL_FAVORITES:                   name = L"Favorites";                   break;
        case CSIDL_FONTS:                       name = L"Fonts";                       break;
        case CSIDL_HISTORY:                     name = L"History";                     break;
        case CSIDL_INTERNET_CACHE:              name = L"Cache";                       break;
        case CSIDL_LOCAL_APPDATA:               name = L"Local AppData";               break;
        case CSIDL_MYMUSIC:                     name = L"My Music";                    break;
        case CSIDL_MYPICTURES:                  name = L"My Pictures";                 break;
        case CSIDL_MYVIDEO:                     name = L"My Video";                    break;
        case CSIDL_NETHOOD:                     name = L"NetHood";                     break;
        case CSIDL_PERSONAL:                    name = L"Personal";                    break;
        case CSIDL_PRINTHOOD:                   name = L"PrintHood";                   break;
        case CSIDL_PROGRAMS:                    name = L"Programs";                    break;
        case CSIDL_RECENT:                      name = L"Recent";                      break;
        case CSIDL_SENDTO:                      name = L"SendTo";                      break;
        case CSIDL_STARTMENU:                   name = L"Start Menu";                  break;
        case CSIDL_STARTUP:                     name = L"Startup";                     break;
        case CSIDL_TEMPLATES:                   name = L"Templates";                   break;
            /* XXX L"Local Settings" */
        }

        if (!name) {
            root = HKEY_LOCAL_MACHINE;
            switch (folder) {
            case CSIDL_COMMON_ADMINTOOLS:       name = L"Common Administrative Tools"; break;
            case CSIDL_COMMON_APPDATA:          name = L"Common AppData";              break;
            case CSIDL_COMMON_DESKTOPDIRECTORY: name = L"Common Desktop";              break;
            case CSIDL_COMMON_DOCUMENTS:        name = L"Common Documents";            break;
            case CSIDL_COMMON_FAVORITES:        name = L"Common Favorites";            break;
            case CSIDL_COMMON_PROGRAMS:         name = L"Common Programs";             break;
            case CSIDL_COMMON_STARTMENU:        name = L"Common Start Menu";           break;
            case CSIDL_COMMON_STARTUP:          name = L"Common Startup";              break;
            case CSIDL_COMMON_TEMPLATES:        name = L"Common Templates";            break;
            case CSIDL_COMMON_MUSIC:            name = L"CommonMusic";                 break;
            case CSIDL_COMMON_PICTURES:         name = L"CommonPictures";              break;
            case CSIDL_COMMON_VIDEO:            name = L"CommonVideo";                 break;
            }
        }
        /* XXX todo
         * case CSIDL_SYSTEM               # GetSystemDirectory()
         * case CSIDL_RESOURCES            # %windir%\Resources\, For theme and other windows resources.
         * case CSIDL_RESOURCES_LOCALIZED  # %windir%\Resources\<LangID>, for theme and other windows specific resources.
         */

#define SHELL_FOLDERS "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders"

        if (name && RegOpenKeyEx(root, SHELL_FOLDERS, 0, KEY_QUERY_VALUE, &hkey) == ERROR_SUCCESS) {
            WCHAR data[MAX_PATH+1];
            DWORD cb = sizeof(data)-sizeof(WCHAR);
            DWORD type = REG_NONE;
            long rc = RegQueryValueExW(hkey, name, NULL, &type, (BYTE*)&data, &cb);
            RegCloseKey(hkey);
            if (rc == ERROR_SUCCESS && type == REG_SZ && cb > sizeof(WCHAR) && data[0]) {
                /* Make sure the string is properly terminated */
                data[cb/sizeof(WCHAR)] = '\0';
                ST(0) = wstr_to_ansipath(aTHX_ data);
                XSRETURN(1);
            }
        }

#undef SHELL_FOLDERS

        /* Unders some circumstances the registry entries seem to have a null string
         * as their value even when the directory already exists.  The environment
         * variables do get set though, so try re-create a Unicode environment and
         * check if they are there.
         */
        sv = NULL;
        switch (folder) {
        case CSIDL_APPDATA:              sv = get_unicode_env(aTHX_ L"APPDATA");            break;
        case CSIDL_PROFILE:              sv = get_unicode_env(aTHX_ L"USERPROFILE");        break;
        case CSIDL_PROGRAM_FILES:        sv = get_unicode_env(aTHX_ L"ProgramFiles");       break;
        case CSIDL_PROGRAM_FILES_COMMON: sv = get_unicode_env(aTHX_ L"CommonProgramFiles"); break;
        case CSIDL_WINDOWS:              sv = get_unicode_env(aTHX_ L"SystemRoot");         break;
        }
        if (sv) {
            ST(0) = sv;
            XSRETURN(1);
        }
    }

    XSRETURN_UNDEF;
}

XS(w32_GetFileVersion)
{
    dXSARGS;
    DWORD size;
    DWORD handle;
    char *filename;
    char *data;

    if (items != 1)
	croak("usage: Win32::GetFileVersion($filename)\n");

    filename = SvPV_nolen(ST(0));
    size = GetFileVersionInfoSize(filename, &handle);
    if (!size)
        XSRETURN_UNDEF;

    New(0, data, size, char);
    if (!data)
        XSRETURN_UNDEF;

    if (GetFileVersionInfo(filename, handle, size, data)) {
        VS_FIXEDFILEINFO *info;
        UINT len;
        if (VerQueryValue(data, "\\", (void**)&info, &len)) {
            int dwValueMS1 = (info->dwFileVersionMS>>16);
            int dwValueMS2 = (info->dwFileVersionMS&0xffff);
            int dwValueLS1 = (info->dwFileVersionLS>>16);
            int dwValueLS2 = (info->dwFileVersionLS&0xffff);

            if (GIMME_V == G_ARRAY) {
                EXTEND(SP, 4);
                XST_mIV(0, dwValueMS1);
                XST_mIV(1, dwValueMS2);
                XST_mIV(2, dwValueLS1);
                XST_mIV(3, dwValueLS2);
                items = 4;
            }
            else {
                char version[50];
                sprintf(version, "%d.%d.%d.%d", dwValueMS1, dwValueMS2, dwValueLS1, dwValueLS2);
                XST_mPV(0, version);
            }
        }
    }
    else
        items = 0;

    Safefree(data);
    XSRETURN(items);
}

#ifdef __CYGWIN__
XS(w32_SetChildShowWindow)
{
    /* This function doesn't do anything useful for cygwin.  In the
     * MSWin32 case it modifies w32_showwindow, which is used by
     * win32_spawnvp().  Since w32_showwindow is an internal variable
     * inside the thread_intern structure, the MSWin32 implementation
     * lives in win32/win32.c in the core Perl distribution.
     */
    dXSARGS;
    XSRETURN_UNDEF;
}
#endif

XS(w32_GetCwd)
{
    dXSARGS;
    /* Make the host for current directory */
    char* ptr = PerlEnv_get_childdir();
    /*
     * If ptr != Nullch
     *   then it worked, set PV valid,
     *   else return 'undef'
     */
    if (ptr) {
	SV *sv = sv_newmortal();
	sv_setpv(sv, ptr);
	PerlEnv_free_childdir(ptr);

#ifndef INCOMPLETE_TAINTS
	SvTAINTED_on(sv);
#endif

	EXTEND(SP,1);
	ST(0) = sv;
	XSRETURN(1);
    }
    XSRETURN_UNDEF;
}

XS(w32_SetCwd)
{
    dXSARGS;
    if (items != 1)
	Perl_croak(aTHX_ "usage: Win32::SetCwd($cwd)");

    if (IsWin2000() && SvUTF8(ST(0))) {
        WCHAR *wide = sv_to_wstr(aTHX_ ST(0));
        char *ansi = my_ansipath(wide);
        int rc = PerlDir_chdir(ansi);
        Safefree(wide);
        Safefree(ansi);
        if (!rc)
            XSRETURN_YES;
    }
    else {
        if (!PerlDir_chdir(SvPV_nolen(ST(0))))
            XSRETURN_YES;
    }

    XSRETURN_NO;
}

XS(w32_GetNextAvailDrive)
{
    dXSARGS;
    char ix = 'C';
    char root[] = "_:\\";

    EXTEND(SP,1);
    while (ix <= 'Z') {
	root[0] = ix++;
	if (GetDriveType(root) == 1) {
	    root[2] = '\0';
	    XSRETURN_PV(root);
	}
    }
    XSRETURN_UNDEF;
}

XS(w32_GetLastError)
{
    dXSARGS;
    EXTEND(SP,1);
    XSRETURN_IV(GetLastError());
}

XS(w32_SetLastError)
{
    dXSARGS;
    if (items != 1)
	Perl_croak(aTHX_ "usage: Win32::SetLastError($error)");
    SetLastError((DWORD)SvIV(ST(0)));
    XSRETURN_EMPTY;
}

XS(w32_LoginName)
{
    dXSARGS;
    EXTEND(SP,1);
    if (IsWin2000()) {
        WCHAR name[128];
        DWORD size = countof(name);
        if (GetUserNameW(name, &size)) {
            ST(0) = wstr_to_sv(aTHX_ name);
            XSRETURN(1);
        }
    }
    else {
        char name[128];
        DWORD size = countof(name);
        if (GetUserNameA(name, &size)) {
            /* size includes NULL */
            ST(0) = sv_2mortal(newSVpvn(name, size-1));
            XSRETURN(1);
        }
    }
    XSRETURN_UNDEF;
}

XS(w32_NodeName)
{
    dXSARGS;
    char name[MAX_COMPUTERNAME_LENGTH+1];
    DWORD size = sizeof(name);
    EXTEND(SP,1);
    if (GetComputerName(name,&size)) {
	/* size does NOT include NULL :-( */
	ST(0) = sv_2mortal(newSVpvn(name,size));
	XSRETURN(1);
    }
    XSRETURN_UNDEF;
}


XS(w32_DomainName)
{
    dXSARGS;
    HMODULE module = LoadLibrary("netapi32.dll");
    PFNNetApiBufferFree pfnNetApiBufferFree;
    PFNNetWkstaGetInfo pfnNetWkstaGetInfo;

    if (module) {
        GETPROC(NetApiBufferFree);
        GETPROC(NetWkstaGetInfo);
    }
    EXTEND(SP,1);
    if (module && pfnNetWkstaGetInfo && pfnNetApiBufferFree) {
	/* this way is more reliable, in case user has a local account. */
	char dname[256];
	DWORD dnamelen = sizeof(dname);
	struct {
	    DWORD   wki100_platform_id;
	    LPWSTR  wki100_computername;
	    LPWSTR  wki100_langroup;
	    DWORD   wki100_ver_major;
	    DWORD   wki100_ver_minor;
	} *pwi;
	DWORD retval;
	retval = pfnNetWkstaGetInfo(NULL, 100, &pwi);
	/* NERR_Success *is* 0*/
	if (retval == 0) {
	    if (pwi->wki100_langroup && *(pwi->wki100_langroup)) {
		WideCharToMultiByte(CP_ACP, 0, pwi->wki100_langroup,
				    -1, (LPSTR)dname, dnamelen, NULL, NULL);
	    }
	    else {
		WideCharToMultiByte(CP_ACP, 0, pwi->wki100_computername,
				    -1, (LPSTR)dname, dnamelen, NULL, NULL);
	    }
	    pfnNetApiBufferFree(pwi);
	    FreeLibrary(module);
	    XSRETURN_PV(dname);
	}
	FreeLibrary(module);
	SetLastError(retval);
    }
    else {
	/* Win95 doesn't have NetWksta*(), so do it the old way */
	char name[256];
	DWORD size = sizeof(name);
	if (module)
	    FreeLibrary(module);
	if (GetUserName(name,&size)) {
	    char sid[ONE_K_BUFSIZE];
	    DWORD sidlen = sizeof(sid);
	    char dname[256];
	    DWORD dnamelen = sizeof(dname);
	    SID_NAME_USE snu;
	    if (LookupAccountName(NULL, name, (PSID)&sid, &sidlen,
				  dname, &dnamelen, &snu)) {
		XSRETURN_PV(dname);		/* all that for this */
	    }
	}
    }
    XSRETURN_UNDEF;
}

XS(w32_FsType)
{
    dXSARGS;
    char fsname[256];
    DWORD flags, filecomplen;
    if (GetVolumeInformation(NULL, NULL, 0, NULL, &filecomplen,
			 &flags, fsname, sizeof(fsname))) {
	if (GIMME_V == G_ARRAY) {
	    XPUSHs(sv_2mortal(newSVpvn(fsname,strlen(fsname))));
	    XPUSHs(sv_2mortal(newSViv(flags)));
	    XPUSHs(sv_2mortal(newSViv(filecomplen)));
	    PUTBACK;
	    return;
	}
	EXTEND(SP,1);
	XSRETURN_PV(fsname);
    }
    XSRETURN_EMPTY;
}

XS(w32_GetOSVersion)
{
    dXSARGS;

    if (GIMME_V == G_SCALAR) {
        XSRETURN_IV(g_osver.dwPlatformId);
    }
    XPUSHs(sv_2mortal(newSVpvn(g_osver.szCSDVersion, strlen(g_osver.szCSDVersion))));

    XPUSHs(sv_2mortal(newSViv(g_osver.dwMajorVersion)));
    XPUSHs(sv_2mortal(newSViv(g_osver.dwMinorVersion)));
    XPUSHs(sv_2mortal(newSViv(g_osver.dwBuildNumber)));
    XPUSHs(sv_2mortal(newSViv(g_osver.dwPlatformId)));
    if (g_osver_ex) {
        XPUSHs(sv_2mortal(newSViv(g_osver.wServicePackMajor)));
        XPUSHs(sv_2mortal(newSViv(g_osver.wServicePackMinor)));
        XPUSHs(sv_2mortal(newSViv(g_osver.wSuiteMask)));
        XPUSHs(sv_2mortal(newSViv(g_osver.wProductType)));
    }
    PUTBACK;
}

XS(w32_IsWinNT)
{
    dXSARGS;
    EXTEND(SP,1);
    XSRETURN_IV(IsWinNT());
}

XS(w32_IsWin95)
{
    dXSARGS;
    EXTEND(SP,1);
    XSRETURN_IV(IsWin95());
}

XS(w32_FormatMessage)
{
    dXSARGS;
    DWORD source = 0;
    char msgbuf[ONE_K_BUFSIZE];

    if (items != 1)
	Perl_croak(aTHX_ "usage: Win32::FormatMessage($errno)");

    if (FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM,
                       &source, (DWORD)SvIV(ST(0)), 0,
                       msgbuf, sizeof(msgbuf)-1, NULL))
    {
        XSRETURN_PV(msgbuf);
    }

    XSRETURN_UNDEF;
}

XS(w32_Spawn)
{
    dXSARGS;
    char *cmd, *args;
    void *env;
    char *dir;
    PROCESS_INFORMATION stProcInfo;
    STARTUPINFO stStartInfo;
    BOOL bSuccess = FALSE;

    if (items != 3)
	Perl_croak(aTHX_ "usage: Win32::Spawn($cmdName, $args, $PID)");

    cmd = SvPV_nolen(ST(0));
    args = SvPV_nolen(ST(1));

    env = PerlEnv_get_childenv();
    dir = PerlEnv_get_childdir();

    memset(&stStartInfo, 0, sizeof(stStartInfo));   /* Clear the block */
    stStartInfo.cb = sizeof(stStartInfo);	    /* Set the structure size */
    stStartInfo.dwFlags = STARTF_USESHOWWINDOW;	    /* Enable wShowWindow control */
    stStartInfo.wShowWindow = SW_SHOWMINNOACTIVE;   /* Start min (normal) */

    if (CreateProcess(
		cmd,			/* Image path */
		args,	 		/* Arguments for command line */
		NULL,			/* Default process security */
		NULL,			/* Default thread security */
		FALSE,			/* Must be TRUE to use std handles */
		NORMAL_PRIORITY_CLASS,	/* No special scheduling */
		env,			/* Inherit our environment block */
		dir,			/* Inherit our currrent directory */
		&stStartInfo,		/* -> Startup info */
		&stProcInfo))		/* <- Process info (if OK) */
    {
	int pid = (int)stProcInfo.dwProcessId;
	if (IsWin95() && pid < 0)
	    pid = -pid;
	sv_setiv(ST(2), pid);
	CloseHandle(stProcInfo.hThread);/* library source code does this. */
	bSuccess = TRUE;
    }
    PerlEnv_free_childenv(env);
    PerlEnv_free_childdir(dir);
    XSRETURN_IV(bSuccess);
}

XS(w32_GetTickCount)
{
    dXSARGS;
    DWORD msec = GetTickCount();
    EXTEND(SP,1);
    if ((IV)msec > 0)
	XSRETURN_IV(msec);
    XSRETURN_NV(msec);
}

XS(w32_GetShortPathName)
{
    dXSARGS;
    SV *shortpath;
    DWORD len;

    if (items != 1)
	Perl_croak(aTHX_ "usage: Win32::GetShortPathName($longPathName)");

    if (IsWin2000()) {
        WCHAR wshort[MAX_PATH+1];
        WCHAR *wlong = sv_to_wstr(aTHX_ ST(0));
        len = GetShortPathNameW(wlong, wshort, countof(wshort));
        Safefree(wlong);
        if (len && len < sizeof(wshort)) {
            ST(0) = wstr_to_sv(aTHX_ wshort);
            XSRETURN(1);
        }
        XSRETURN_UNDEF;
    }

    shortpath = sv_mortalcopy(ST(0));
    SvUPGRADE(shortpath, SVt_PV);
    if (!SvPVX(shortpath) || !SvLEN(shortpath))
        XSRETURN_UNDEF;

    /* src == target is allowed */
    do {
	len = GetShortPathName(SvPVX(shortpath),
			       SvPVX(shortpath),
			       (DWORD)SvLEN(shortpath));
    } while (len >= SvLEN(shortpath) && sv_grow(shortpath,len+1));
    if (len) {
	SvCUR_set(shortpath,len);
	*SvEND(shortpath) = '\0';
	ST(0) = shortpath;
	XSRETURN(1);
    }
    XSRETURN_UNDEF;
}

XS(w32_GetFullPathName)
{
    dXSARGS;
    char *fullname;
    char *ansi = NULL;

/* The code below relies on the fact that PerlDir_mapX() returns an
 * absolute path, which is only true under PERL_IMPLICIT_SYS when
 * we use the virtualization code from win32/vdir.h.
 * Without it PerlDir_mapX() is a no-op and we need to use the same
 * code as we use for Cygwin.
 */
#if __CYGWIN__ || !defined(PERL_IMPLICIT_SYS)
    char buffer[2*MAX_PATH];
#endif

    if (items != 1)
	Perl_croak(aTHX_ "usage: Win32::GetFullPathName($filename)");

#if __CYGWIN__ || !defined(PERL_IMPLICIT_SYS)
    if (IsWin2000()) {
        WCHAR *filename = sv_to_wstr(aTHX_ ST(0));
        WCHAR full[2*MAX_PATH];
        DWORD len = GetFullPathNameW(filename, countof(full), full, NULL);
        Safefree(filename);
        if (len == 0 || len >= countof(full))
            XSRETURN_EMPTY;
        ansi = fullname = my_ansipath(full);
    }
    else {
        DWORD len = GetFullPathNameA(SvPV_nolen(ST(0)), countof(buffer), buffer, NULL);
        if (len == 0 || len >= countof(buffer))
            XSRETURN_EMPTY;
        fullname = buffer;
    }
#else
    /* Don't use my_ansipath() unless the $filename argument is in Unicode.
     * If the relative path doesn't exist, GetShortPathName() will fail and
     * my_ansipath() will use the long name with replacement characters.
     * In that case we will be better off using PerlDir_mapA(), which
     * already uses the ANSI name of the current directory.
     *
     * XXX The one missing case is where we could downgrade $filename
     * XXX from UTF8 into the current codepage.
     */
    if (IsWin2000() && SvUTF8(ST(0))) {
        WCHAR *filename = sv_to_wstr(aTHX_ ST(0));
        WCHAR *mappedname = PerlDir_mapW(filename);
        Safefree(filename);
        ansi = fullname = my_ansipath(mappedname);
    }
    else {
        fullname = PerlDir_mapA(SvPV_nolen(ST(0)));
    }
#  if PERL_VERSION < 8
    {
        /* PerlDir_mapX() in Perl 5.6 used to return forward slashes */
        char *str = fullname;
        while (*str) {
            if (*str == '/')
                *str = '\\';
            ++str;
        }
    }
#  endif
#endif

    /* GetFullPathName() on Windows NT drops trailing backslash */
    if (g_osver.dwMajorVersion == 4 && *fullname) {
        STRLEN len;
        char *pv = SvPV(ST(0), len);
        char *lastchar = fullname + strlen(fullname) - 1;
        /* If ST(0) ends with a slash, but fullname doesn't ... */
        if (len && (pv[len-1] == '/' || pv[len-1] == '\\') && *lastchar != '\\') {
            /* fullname is the MAX_PATH+1 sized buffer returned from PerlDir_mapA()
             * or the 2*MAX_PATH sized local buffer in the __CYGWIN__ case.
             */
            strcpy(lastchar+1, "\\");
        }
    }

    if (GIMME_V == G_ARRAY) {
        char *filepart = strrchr(fullname, '\\');

        EXTEND(SP,1);
        if (filepart) {
            XST_mPV(1, ++filepart);
            *filepart = '\0';
        }
        else {
            XST_mPVN(1, "", 0);
        }
        items = 2;
    }
    XST_mPV(0, fullname);

    if (ansi)
        Safefree(ansi);
    XSRETURN(items);
}

XS(w32_GetLongPathName)
{
    dXSARGS;

    if (items != 1)
	Perl_croak(aTHX_ "usage: Win32::GetLongPathName($pathname)");

    if (IsWin2000()) {
        WCHAR *wstr = sv_to_wstr(aTHX_ ST(0));
        WCHAR wide_path[MAX_PATH+1];
        WCHAR *long_path;

        wcscpy(wide_path, wstr);
        Safefree(wstr);
        long_path = my_longpathW(wide_path);
        if (long_path) {
            ST(0) = wstr_to_sv(aTHX_ long_path);
            XSRETURN(1);
        }
    }
    else {
        SV *path;
        char tmpbuf[MAX_PATH+1];
        char *pathstr;
        STRLEN len;

        path = ST(0);
        pathstr = SvPV(path,len);
        strcpy(tmpbuf, pathstr);
        pathstr = my_longpathA(tmpbuf);
        if (pathstr) {
            ST(0) = sv_2mortal(newSVpvn(pathstr, strlen(pathstr)));
            XSRETURN(1);
        }
    }
    XSRETURN_EMPTY;
}

XS(w32_GetANSIPathName)
{
    dXSARGS;
    WCHAR *wide_path;

    if (items != 1)
	Perl_croak(aTHX_ "usage: Win32::GetANSIPathName($pathname)");

    wide_path = sv_to_wstr(aTHX_ ST(0));
    ST(0) = wstr_to_ansipath(aTHX_ wide_path);
    Safefree(wide_path);
    XSRETURN(1);
}

XS(w32_Sleep)
{
    dXSARGS;
    if (items != 1)
	Perl_croak(aTHX_ "usage: Win32::Sleep($milliseconds)");
    Sleep((DWORD)SvIV(ST(0)));
    XSRETURN_YES;
}

XS(w32_CopyFile)
{
    dXSARGS;
    BOOL bResult;
    char szSourceFile[MAX_PATH+1];

    if (items != 3)
	Perl_croak(aTHX_ "usage: Win32::CopyFile($from, $to, $overwrite)");
    strcpy(szSourceFile, PerlDir_mapA(SvPV_nolen(ST(0))));
    bResult = CopyFileA(szSourceFile, PerlDir_mapA(SvPV_nolen(ST(1))), !SvTRUE(ST(2)));
    if (bResult)
	XSRETURN_YES;
    XSRETURN_NO;
}

XS(w32_OutputDebugString)
{
    dXSARGS;
    if (items != 1)
	Perl_croak(aTHX_ "usage: Win32::OutputDebugString($string)");

    if (SvUTF8(ST(0))) {
        WCHAR *str = sv_to_wstr(aTHX_ ST(0));
        OutputDebugStringW(str);
        Safefree(str);
    }
    else
        OutputDebugStringA(SvPV_nolen(ST(0)));

    XSRETURN_EMPTY;
}

XS(w32_GetCurrentProcessId)
{
    dXSARGS;
    EXTEND(SP,1);
    XSRETURN_IV(GetCurrentProcessId());
}

XS(w32_GetCurrentThreadId)
{
    dXSARGS;
    EXTEND(SP,1);
    XSRETURN_IV(GetCurrentThreadId());
}

XS(w32_CreateDirectory)
{
    dXSARGS;
    BOOL result;

    if (items != 1)
	Perl_croak(aTHX_ "usage: Win32::CreateDirectory($dir)");

    if (IsWin2000() && SvUTF8(ST(0))) {
        WCHAR *dir = sv_to_wstr(aTHX_ ST(0));
        result = CreateDirectoryW(dir, NULL);
        Safefree(dir);
    }
    else {
        result = CreateDirectoryA(SvPV_nolen(ST(0)), NULL);
    }

    ST(0) = boolSV(result);
    XSRETURN(1);
}

XS(w32_CreateFile)
{
    dXSARGS;
    HANDLE handle;

    if (items != 1)
	Perl_croak(aTHX_ "usage: Win32::CreateFile($file)");

    if (IsWin2000() && SvUTF8(ST(0))) {
        WCHAR *file = sv_to_wstr(aTHX_ ST(0));
        handle = CreateFileW(file, GENERIC_WRITE, FILE_SHARE_WRITE,
                             NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        Safefree(file);
    }
    else {
        handle = CreateFileA(SvPV_nolen(ST(0)), GENERIC_WRITE, FILE_SHARE_WRITE,
                             NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    }

    if (handle != INVALID_HANDLE_VALUE)
        CloseHandle(handle);

    ST(0) = boolSV(handle != INVALID_HANDLE_VALUE);
    XSRETURN(1);
}

MODULE = Win32            PACKAGE = Win32

PROTOTYPES: DISABLE

BOOT:
{
    char *file = __FILE__;

    if (g_osver.dwOSVersionInfoSize == 0) {
        g_osver.dwOSVersionInfoSize = sizeof(g_osver);
        if (!GetVersionExA((OSVERSIONINFOA*)&g_osver)) {
            g_osver_ex = FALSE;
            g_osver.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
            GetVersionExA((OSVERSIONINFOA*)&g_osver);
        }
    }

    newXS("Win32::LookupAccountName", w32_LookupAccountName, file);
    newXS("Win32::LookupAccountSID", w32_LookupAccountSID, file);
    newXS("Win32::InitiateSystemShutdown", w32_InitiateSystemShutdown, file);
    newXS("Win32::AbortSystemShutdown", w32_AbortSystemShutdown, file);
    newXS("Win32::ExpandEnvironmentStrings", w32_ExpandEnvironmentStrings, file);
    newXS("Win32::MsgBox", w32_MsgBox, file);
    newXS("Win32::LoadLibrary", w32_LoadLibrary, file);
    newXS("Win32::FreeLibrary", w32_FreeLibrary, file);
    newXS("Win32::GetProcAddress", w32_GetProcAddress, file);
    newXS("Win32::RegisterServer", w32_RegisterServer, file);
    newXS("Win32::UnregisterServer", w32_UnregisterServer, file);
    newXS("Win32::GetArchName", w32_GetArchName, file);
    newXS("Win32::GetChipName", w32_GetChipName, file);
    newXS("Win32::GuidGen", w32_GuidGen, file);
    newXS("Win32::GetFolderPath", w32_GetFolderPath, file);
    newXS("Win32::IsAdminUser", w32_IsAdminUser, file);
    newXS("Win32::GetFileVersion", w32_GetFileVersion, file);

    newXS("Win32::GetCwd", w32_GetCwd, file);
    newXS("Win32::SetCwd", w32_SetCwd, file);
    newXS("Win32::GetNextAvailDrive", w32_GetNextAvailDrive, file);
    newXS("Win32::GetLastError", w32_GetLastError, file);
    newXS("Win32::SetLastError", w32_SetLastError, file);
    newXS("Win32::LoginName", w32_LoginName, file);
    newXS("Win32::NodeName", w32_NodeName, file);
    newXS("Win32::DomainName", w32_DomainName, file);
    newXS("Win32::FsType", w32_FsType, file);
    newXS("Win32::GetOSVersion", w32_GetOSVersion, file);
    newXS("Win32::IsWinNT", w32_IsWinNT, file);
    newXS("Win32::IsWin95", w32_IsWin95, file);
    newXS("Win32::FormatMessage", w32_FormatMessage, file);
    newXS("Win32::Spawn", w32_Spawn, file);
    newXS("Win32::GetTickCount", w32_GetTickCount, file);
    newXS("Win32::GetShortPathName", w32_GetShortPathName, file);
    newXS("Win32::GetFullPathName", w32_GetFullPathName, file);
    newXS("Win32::GetLongPathName", w32_GetLongPathName, file);
    newXS("Win32::GetANSIPathName", w32_GetANSIPathName, file);
    newXS("Win32::CopyFile", w32_CopyFile, file);
    newXS("Win32::Sleep", w32_Sleep, file);
    newXS("Win32::OutputDebugString", w32_OutputDebugString, file);
    newXS("Win32::GetCurrentProcessId", w32_GetCurrentProcessId, file);
    newXS("Win32::GetCurrentThreadId", w32_GetCurrentThreadId, file);
    newXS("Win32::CreateDirectory", w32_CreateDirectory, file);
    newXS("Win32::CreateFile", w32_CreateFile, file);
#ifdef __CYGWIN__
    newXS("Win32::SetChildShowWindow", w32_SetChildShowWindow, file);
#endif
    XSRETURN_YES;
}
