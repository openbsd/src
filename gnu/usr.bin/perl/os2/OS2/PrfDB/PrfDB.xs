#define INCL_WINSHELLDATA /* Or use INCL_WIN, INCL_PM, */

#ifdef __cplusplus
extern "C" {
#endif
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include <os2.h>
#ifdef __cplusplus
}
#endif

#define Prf_Open(pszFileName) SaveWinError(PrfOpenProfile(Perl_hab, (pszFileName)))
#define Prf_Close(hini) (!CheckWinError(PrfCloseProfile(hini)))

SV *
Prf_Get(pTHX_ HINI hini, PSZ app, PSZ key) {
    ULONG len;
    BOOL rc;
    SV *sv;

    if (CheckWinError(PrfQueryProfileSize(hini, app, key, &len))) return &PL_sv_undef;
    sv = newSVpv("", 0);
    SvGROW(sv, len + 1);
    if (CheckWinError(PrfQueryProfileData(hini, app, key, SvPVX(sv), &len))
	|| (len == 0 && (app == NULL || key == NULL))) { /* Somewhy needed. */
	SvREFCNT_dec(sv);
	return &PL_sv_undef;
    }
    SvCUR_set(sv, len);
    *SvEND(sv) = 0;
    return sv;
}

I32
Prf_GetLength(HINI hini, PSZ app, PSZ key) {
    U32 len;

    if (CheckWinError(PrfQueryProfileSize(hini, app, key, &len))) return -1;
    return len;
}

#define Prf_Set(hini, app, key, s, l)			\
	 (!(CheckWinError(PrfWriteProfileData(hini, app, key, s, l))))

#define Prf_System(key)					\
	( (key) ? ( (key) == 1  ? HINI_USERPROFILE	\
				: ( (key) == 2 ? HINI_SYSTEMPROFILE \
						: (die("Wrong profile id %i", key), 0) )) \
	  : HINI_PROFILE)

SV*
Prf_Profiles(pTHX)
{
    AV *av = newAV();
    SV *rv;
    char user[257];
    char system[257];
    PRFPROFILE info = { 257, user, 257, system};
    
    if (CheckWinError(PrfQueryProfile(Perl_hab, &info))) return &PL_sv_undef;
    if (info.cchUserName > 257 || info.cchSysName > 257)
	die("Panic: Profile names too long");
    av_push(av, newSVpv(user, info.cchUserName - 1));
    av_push(av, newSVpv(system, info.cchSysName - 1));
    rv = newRV((SV*)av);
    SvREFCNT_dec(av);
    return rv;
}

BOOL
Prf_SetUser(pTHX_ SV *sv)
{
    char user[257];
    char system[257];
    PRFPROFILE info = { 257, user, 257, system};
    
    if (!SvPOK(sv)) die("User profile name not defined");
    if (SvCUR(sv) > 256) die("User profile name too long");
    if (CheckWinError(PrfQueryProfile(Perl_hab, &info))) return 0;
    if (info.cchSysName > 257)
	die("Panic: System profile name too long");
    info.cchUserName = SvCUR(sv) + 1;
    info.pszUserName = SvPVX(sv);
    return !CheckWinError(PrfReset(Perl_hab, &info));
}

MODULE = OS2::PrfDB		PACKAGE = OS2::Prf PREFIX = Prf_

HINI
Prf_Open(pszFileName)
 PSZ     pszFileName;

BOOL
Prf_Close(hini)
 HINI     hini;

SV *
Prf_Get(hini, app, key)
 HINI hini;
 PSZ app;
 PSZ key;
CODE:
    RETVAL = Prf_Get(aTHX_ hini, app, key);
OUTPUT:
    RETVAL

int
Prf_Set(hini, app, key, s, l = (SvPOK(ST(3)) ? SvCUR(ST(3)): -1))
 HINI hini;
 PSZ app;
 PSZ key;
 PSZ s;
 ULONG l;

I32
Prf_GetLength(hini, app, key)
 HINI hini;
 PSZ app;
 PSZ key;

HINI
Prf_System(key)
 int key;

SV*
Prf_Profiles()
CODE:
    RETVAL = Prf_Profiles(aTHX);
OUTPUT:
    RETVAL

BOOL
Prf_SetUser(sv)
 SV *sv
CODE:
    RETVAL = Prf_SetUser(aTHX_ sv);
OUTPUT:
    RETVAL

BOOT:
	Acquire_hab();
