#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <process.h>
#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>

static unsigned long
constant(char *name, int arg)
{
    errno = 0;
    if (name[0] == 'P' && name[1] == '_') {
	if (strEQ(name, "P_BACKGROUND"))
#ifdef P_BACKGROUND
	    return P_BACKGROUND;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_DEBUG"))
#ifdef P_DEBUG
	    return P_DEBUG;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_DEFAULT"))
#ifdef P_DEFAULT
	    return P_DEFAULT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_DETACH"))
#ifdef P_DETACH
	    return P_DETACH;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_FOREGROUND"))
#ifdef P_FOREGROUND
	    return P_FOREGROUND;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_FULLSCREEN"))
#ifdef P_FULLSCREEN
	    return P_FULLSCREEN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_MAXIMIZE"))
#ifdef P_MAXIMIZE
	    return P_MAXIMIZE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_MINIMIZE"))
#ifdef P_MINIMIZE
	    return P_MINIMIZE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_NOCLOSE"))
#ifdef P_NOCLOSE
	    return P_NOCLOSE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_NOSESSION"))
#ifdef P_NOSESSION
	    return P_NOSESSION;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_NOWAIT"))
#ifdef P_NOWAIT
	    return P_NOWAIT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_OVERLAY"))
#ifdef P_OVERLAY
	    return P_OVERLAY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_PM"))
#ifdef P_PM
	    return P_PM;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_QUOTE"))
#ifdef P_QUOTE
	    return P_QUOTE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_SESSION"))
#ifdef P_SESSION
	    return P_SESSION;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_TILDE"))
#ifdef P_TILDE
	    return P_TILDE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_UNRELATED"))
#ifdef P_UNRELATED
	    return P_UNRELATED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_WAIT"))
#ifdef P_WAIT
	    return P_WAIT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_WINDOWED"))
#ifdef P_WINDOWED
	    return P_WINDOWED;
#else
	    goto not_there;
#endif
    } else if (name[0] == 'T' && name[1] == '_') {
	if (strEQ(name, "FAPPTYP_NOTSPEC"))
#ifdef FAPPTYP_NOTSPEC
	    return FAPPTYP_NOTSPEC;
#else
	    goto not_there;
#endif
	if (strEQ(name, "T_NOTWINDOWCOMPAT"))
#ifdef FAPPTYP_NOTWINDOWCOMPAT
	    return FAPPTYP_NOTWINDOWCOMPAT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "T_WINDOWCOMPAT"))
#ifdef FAPPTYP_WINDOWCOMPAT
	    return FAPPTYP_WINDOWCOMPAT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "T_WINDOWAPI"))
#ifdef FAPPTYP_WINDOWAPI
	    return FAPPTYP_WINDOWAPI;
#else
	    goto not_there;
#endif
	if (strEQ(name, "T_BOUND"))
#ifdef FAPPTYP_BOUND
	    return FAPPTYP_BOUND;
#else
	    goto not_there;
#endif
	if (strEQ(name, "T_DLL"))
#ifdef FAPPTYP_DLL
	    return FAPPTYP_DLL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "T_DOS"))
#ifdef FAPPTYP_DOS
	    return FAPPTYP_DOS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "T_PHYSDRV"))
#ifdef FAPPTYP_PHYSDRV
	    return FAPPTYP_PHYSDRV;
#else
	    goto not_there;
#endif
	if (strEQ(name, "T_VIRTDRV"))
#ifdef FAPPTYP_VIRTDRV
	    return FAPPTYP_VIRTDRV;
#else
	    goto not_there;
#endif
	if (strEQ(name, "T_PROTDLL"))
#ifdef FAPPTYP_PROTDLL
	    return FAPPTYP_PROTDLL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "T_32BIT"))
#ifdef FAPPTYP_32BIT
	    return FAPPTYP_32BIT;
#else
	    goto not_there;
#endif
    }

    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

const char* const ptypes[] = { "FS", "DOS", "VIO", "PM", "DETACH" };

static char *
my_type()
{
    int rc;
    TIB *tib;
    PIB *pib;
    
    if (!(_emx_env & 0x200)) return (char*)ptypes[1]; /* not OS/2. */
    if (CheckOSError(DosGetInfoBlocks(&tib, &pib))) 
	return NULL; 
    
    return (pib->pib_ultype <= 4 ? (char*)ptypes[pib->pib_ultype] : "UNKNOWN");
}

static ULONG
file_type(char *path)
{
    int rc;
    ULONG apptype;
    
    if (!(_emx_env & 0x200)) 
	croak("file_type not implemented on DOS"); /* not OS/2. */
    if (CheckOSError(DosQueryAppType(path, &apptype))) {
	if (rc == ERROR_INVALID_EXE_SIGNATURE) 
	    croak("Invalid EXE signature"); 
	else if (rc == ERROR_EXE_MARKED_INVALID) {
	    croak("EXE marked invalid"); 
	}
	croak("DosQueryAppType err %ld", rc); 
    }
    
    return apptype;
}

static void
fill_swcntrl(SWCNTRL *swcntrlp)
{
	 int rc;
	 PTIB ptib;
	 PPIB ppib;
	 HSWITCH hSwitch;    
	 HWND hwndMe;

	 if (!(_emx_env & 0x200)) 
	     croak("switch_entry not implemented on DOS"); /* not OS/2. */
	 if (CheckOSError(DosGetInfoBlocks(&ptib, &ppib)))
	     croak("DosGetInfoBlocks err %ld", rc);
	 if (CheckWinError(hSwitch = 
			   WinQuerySwitchHandle(NULLHANDLE, 
						(PID)ppib->pib_ulpid)))
	     croak("WinQuerySwitchHandle err %ld", Perl_rc);
	 if (CheckOSError(WinQuerySwitchEntry(hSwitch, swcntrlp)))
	     croak("WinQuerySwitchEntry err %ld", rc);
}

/* static ULONG (* APIENTRY16 pDosSmSetTitle)(ULONG, PSZ); */
ULONG _THUNK_FUNCTION(DosSmSetTitle)(ULONG, PSZ);

#if 0			/*  Does not work.  */
static ULONG (*pDosSmSetTitle)(ULONG, PSZ);

static void
set_title(char *s)
{
    SWCNTRL swcntrl;
    static HMODULE hdosc = 0;
    BYTE buf[20];
    long rc;

    fill_swcntrl(&swcntrl);
    if (!pDosSmSetTitle || !hdosc) {
	if (CheckOSError(DosLoadModule(buf, sizeof buf, "sesmgr", &hdosc)))
	    croak("Cannot load SESMGR: no `%s'", buf);
	if (CheckOSError(DosQueryProcAddr(hdosc, 0, "DOSSMSETTITLE",
					  (PFN*)&pDosSmSetTitle)))
	    croak("Cannot load SESMGR.DOSSMSETTITLE, err=%ld", rc);
    }
/*     (pDosSmSetTitle)(swcntrl.idSession,s); */
    rc = ((USHORT)
          (_THUNK_PROLOG (2+4);
           _THUNK_SHORT (swcntrl.idSession);
           _THUNK_FLAT (s);
           _THUNK_CALLI (*pDosSmSetTitle)));
    if (CheckOSError(rc))
	warn("*DOSSMSETTITLE: err=%ld, ses=%ld, addr=%x, *paddr=%x", 
	     rc, swcntrl.idSession, &_THUNK_FUNCTION(DosSmSetTitle),
	     pDosSmSetTitle);
}

#else /* !0 */

static bool
set_title(char *s)
{
    SWCNTRL swcntrl;
    static HMODULE hdosc = 0;
    BYTE buf[20];
    long rc;

    fill_swcntrl(&swcntrl);
    rc = ((USHORT)
          (_THUNK_PROLOG (2+4);
           _THUNK_SHORT (swcntrl.idSession);
           _THUNK_FLAT (s);
           _THUNK_CALL (DosSmSetTitle)));
#if 0
    if (CheckOSError(rc))
	warn("DOSSMSETTITLE: err=%ld, ses=%ld, addr=%x", 
	     rc, swcntrl.idSession, _THUNK_FUNCTION(DosSmSetTitle));
#endif
    return !CheckOSError(rc);
}
#endif /* !0 */

#if 0			/*  Does not work.  */
USHORT _THUNK_FUNCTION(Win16SetTitle) ();

static void
set_title2(char *s)
{
    long rc;

    rc = ((USHORT)
          (_THUNK_PROLOG (4);
           _THUNK_FLAT (s);
           _THUNK_CALL (Win16SetTitle)));
    if (CheckWinError(rc))
	warn("Win16SetTitle: err=%ld", rc);
}
#endif

MODULE = OS2::Process		PACKAGE = OS2::Process


unsigned long
constant(name,arg)
	char *		name
	int		arg

char *
my_type()

U32
file_type(path)
    char *path

U32
process_entry()
    PPCODE:
     {
	 SWCNTRL swcntrl;

	 fill_swcntrl(&swcntrl);
	 EXTEND(sp,9);
	 PUSHs(sv_2mortal(newSVpv(swcntrl.szSwtitle, 0)));
	 PUSHs(sv_2mortal(newSVnv(swcntrl.hwnd)));
	 PUSHs(sv_2mortal(newSVnv(swcntrl.hwndIcon)));
	 PUSHs(sv_2mortal(newSViv(swcntrl.hprog)));
	 PUSHs(sv_2mortal(newSViv(swcntrl.idProcess)));
	 PUSHs(sv_2mortal(newSViv(swcntrl.idSession)));
	 PUSHs(sv_2mortal(newSViv(swcntrl.uchVisibility != SWL_INVISIBLE)));
	 PUSHs(sv_2mortal(newSViv(swcntrl.uchVisibility == SWL_GRAYED)));
	 PUSHs(sv_2mortal(newSViv(swcntrl.fbJump == SWL_JUMPABLE)));
	 PUSHs(sv_2mortal(newSViv(swcntrl.bProgType)));
     }

bool
set_title(s)
    char *s
