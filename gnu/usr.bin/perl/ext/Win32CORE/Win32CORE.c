/*    Win32CORE.c
 *
 *    Copyright (C) 2007 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#if defined(__CYGWIN__) && !defined(USEIMPORTLIB)
  #undef WIN32
#endif
#include "EXTERN.h"
#if defined(__CYGWIN__) && !defined(USEIMPORTLIB)
  #define EXTCONST extern const
#endif
#include "perl.h"
#include "XSUB.h"

static void
forward(pTHX_ const char *function)
{
    dXSARGS;
    DWORD err = GetLastError();
    Perl_load_module(aTHX_ PERL_LOADMOD_NOIMPORT, newSVpvn("Win32",5), newSVnv(0.27));
    SetLastError(err);
    SPAGAIN;
    PUSHMARK(SP-items);
    call_pv(function, GIMME_V);
}

#define FORWARD(function) XS(w32_##function){ forward(aTHX_ "Win32::"#function); }
FORWARD(GetCwd)
FORWARD(SetCwd)
FORWARD(GetNextAvailDrive)
FORWARD(GetLastError)
FORWARD(SetLastError)
FORWARD(LoginName)
FORWARD(NodeName)
FORWARD(DomainName)
FORWARD(FsType)
FORWARD(GetOSVersion)
FORWARD(IsWinNT)
FORWARD(IsWin95)
FORWARD(FormatMessage)
FORWARD(Spawn)
FORWARD(GetTickCount)
FORWARD(GetShortPathName)
FORWARD(GetFullPathName)
FORWARD(GetLongPathName)
FORWARD(CopyFile)
FORWARD(Sleep)

/* Don't forward Win32::SetChildShowWindow().  It accesses the internal variable
 * w32_showwindow in thread_intern and is therefore not implemented in Win32.xs.
 */
/* FORWARD(SetChildShowWindow) */

#undef FORWARD

XS(boot_Win32CORE)
{
    /* This function only exists because writemain.SH, lib/ExtUtils/Embed.pm
     * and win32/buildext.pl will all generate references to it.  The function
     * should never be called though, as Win32CORE.pm doesn't use DynaLoader.
     */
}
#if defined(__CYGWIN__) && defined(USEIMPORTLIB)
__declspec(dllexport)
#endif
void
init_Win32CORE(pTHX)
{
    /* This function is called from init_os_extras().  The Perl interpreter
     * is not yet fully initialized, so don't do anything fancy in here.
     */

    char *file = __FILE__;

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
    newXS("Win32::CopyFile", w32_CopyFile, file);
    newXS("Win32::Sleep", w32_Sleep, file);
    /* newXS("Win32::SetChildShowWindow", w32_SetChildShowWindow, file); */
}
