#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#define INCL_BASE
#define INCL_REXXSAA
#include <os2emx.h>

#if 0
#define INCL_REXXSAA
#pragma pack(1)
#define _Packed
#include <rexxsaa.h>
#pragma pack()
#endif

extern ULONG _emx_exception (	EXCEPTIONREPORTRECORD *,
				EXCEPTIONREGISTRATIONRECORD *,
                                CONTEXTRECORD *,
                                void *);

static RXSTRING * strs;
static int	  nstrs;
static SHVBLOCK * vars;
static int	  nvars;
static char *	  trace;

static RXSTRING   rxcommand    = {  9, "RXCOMMAND" };
static RXSTRING   rxsubroutine = { 12, "RXSUBROUTINE" };
static RXSTRING   rxfunction   = { 11, "RXFUNCTION" };

static ULONG PERLCALL(PCSZ name, ULONG argc, PRXSTRING argv, PCSZ queue, PRXSTRING ret);

#if 1
 #define Set	RXSHV_SET
 #define Fetch	RXSHV_FETCH
 #define Drop	RXSHV_DROPV
#else
 #define Set	RXSHV_SYSET
 #define Fetch	RXSHV_SYFET
 #define Drop	RXSHV_SYDRO
#endif

static long incompartment;

static SV*
exec_in_REXX(pTHX_ char *cmd, char * handlerName, RexxFunctionHandler *handler)
{
    HMODULE hRexx, hRexxAPI;
    BYTE    buf[200];
    LONG    APIENTRY (*pRexxStart) (LONG, PRXSTRING, PSZ, PRXSTRING, 
				    PSZ, LONG, PRXSYSEXIT, PSHORT, PRXSTRING);
    APIRET  APIENTRY (*pRexxRegisterFunctionExe) (PSZ,
						  RexxFunctionHandler *);
    APIRET  APIENTRY (*pRexxDeregisterFunction) (PSZ);
    RXSTRING args[1];
    RXSTRING inst[2];
    RXSTRING result;
    USHORT   retcode;
    LONG rc;
    SV *res;

    if (incompartment)
	Perl_die(aTHX_ "Attempt to reenter into REXX compartment");
    incompartment = 1;

    if (DosLoadModule(buf, sizeof buf, "REXX", &hRexx)
	|| DosLoadModule(buf, sizeof buf, "REXXAPI", &hRexxAPI)
	|| DosQueryProcAddr(hRexx, 0, "RexxStart", (PFN *)&pRexxStart)
	|| DosQueryProcAddr(hRexxAPI, 0, "RexxRegisterFunctionExe", 
			    (PFN *)&pRexxRegisterFunctionExe)
	|| DosQueryProcAddr(hRexxAPI, 0, "RexxDeregisterFunction",
			    (PFN *)&pRexxDeregisterFunction)) {
	Perl_die(aTHX_ "REXX not available\n");
    }

    if (handlerName)
	pRexxRegisterFunctionExe(handlerName, handler);

    MAKERXSTRING(args[0], NULL, 0);
    MAKERXSTRING(inst[0], cmd,  strlen(cmd));
    MAKERXSTRING(inst[1], NULL, 0);
    MAKERXSTRING(result,  NULL, 0);
    rc = pRexxStart(0, args, "StartPerl", inst, "Perl", RXSUBROUTINE, NULL,
		    &retcode, &result);

    incompartment = 0;
    pRexxDeregisterFunction("StartPerl");
    DosFreeModule(hRexxAPI);
    DosFreeModule(hRexx);
    if (!RXNULLSTRING(result)) {
	res = newSVpv(RXSTRPTR(result), RXSTRLEN(result));
	DosFreeMem(RXSTRPTR(result));
    } else {
	res = NEWSV(729,0);
    }
    if (rc || SvTRUE(GvSV(PL_errgv))) {
	if (SvTRUE(GvSV(PL_errgv))) {
	    STRLEN n_a;
	    Perl_die(aTHX_ "Error inside perl function called from REXX compartment:\n%s", SvPV(GvSV(PL_errgv), n_a)) ;
	}
	Perl_die(aTHX_ "REXX compartment returned non-zero status %li", rc);
    }

    return res;
}

static SV* exec_cv;

static ULONG
PERLSTART(PCSZ name, ULONG argc, PRXSTRING argv, PCSZ queue, PRXSTRING ret)
{
    return PERLCALL(NULL, argc, argv, queue, ret);
}

#define in_rexx_compartment() exec_in_REXX(aTHX_ "return StartPerl()\r\n", \
					   "StartPerl", PERLSTART)
#define REXX_call(cv) ( exec_cv = (cv), in_rexx_compartment())
#define REXX_eval_with(cmd,name,cv) ( exec_cv = (cv),		\
				      exec_in_REXX(aTHX_ cmd,name,PERLSTART))
#define REXX_eval(cmd) REXX_eval_with(cmd,NULL,NULL)

static ULONG
PERLCALL(PCSZ name, ULONG argc, PRXSTRING argv, PCSZ queue, PRXSTRING ret)
{
    dTHX;
    EXCEPTIONREGISTRATIONRECORD xreg = { NULL, _emx_exception };
    int i, rc;
    unsigned long len;
    char *str;
    char **arr;
    SV *res;
    dSP;

    DosSetExceptionHandler(&xreg);

    ENTER;
    SAVETMPS;
    PUSHMARK(SP);

#if 0
    if (!my_perl) {
	DosUnsetExceptionHandler(&xreg);
	return 1;
    }
#endif 

    for (i = 0; i < argc; ++i)
	XPUSHs(sv_2mortal(newSVpvn(argv[i].strptr, argv[i].strlength)));
    PUTBACK;
    if (name) {
	rc = perl_call_pv(name, G_SCALAR | G_EVAL);
    } else if (exec_cv) {
	SV *cv = exec_cv;

	exec_cv = NULL;
	rc = perl_call_sv(cv, G_SCALAR | G_EVAL);
    } else
	rc = -1;

    SPAGAIN;

    if (rc == 1)			/* must be! */
	res = POPs;
    if (rc == 1 && SvOK(res)) { 
	str = SvPVx(res, len);
	if (len <= 256			/* Default buffer is 256-char long */
	    || !CheckOSError(DosAllocMem((PPVOID)&ret->strptr, len,
					PAG_READ|PAG_WRITE|PAG_COMMIT))) {
	    memcpy(ret->strptr, str, len);
	    ret->strlength = len;
	} else
	    rc = 0;
    } else
	rc = 0;

    PUTBACK ;
    FREETMPS ;
    LEAVE ;

    DosUnsetExceptionHandler(&xreg);
    return rc == 1 ? 0 : 1;			/* 0 means SUCCESS */
}

static void
needstrs(int n)
{
    if (n > nstrs) {
	if (strs)
	    free(strs);
	nstrs = 2 * n;
	strs = malloc(nstrs * sizeof(RXSTRING));
    }
}

static void
needvars(int n)
{
    if (n > nvars) {
	if (vars)
	    free(vars);
	nvars = 2 * n;
	vars = malloc(nvars * sizeof(SHVBLOCK));
    }
}

static void
initialize(void)
{
    needstrs(8);
    needvars(8);
    trace = getenv("PERL_REXX_DEBUG");
}

static int
constant(char *name, int arg)
{
    errno = EINVAL;
    return 0;
}


MODULE = OS2::REXX		PACKAGE = OS2::REXX

BOOT:
	initialize();

int
constant(name,arg)
	char *		name
	int		arg

int
_set(name,value,...)
	char *		name
	char *		value
 CODE:
   {
       int   i;
       int   n = (items + 1) / 2;
       ULONG rc;
       needvars(n);
       if (trace)
	   fprintf(stderr, "REXXCALL::_set");
       for (i = 0; i < n; ++i) {
	   SHVBLOCK * var = &vars[i];
	   STRLEN     namelen;
	   STRLEN     valuelen;
	   name = SvPV(ST(2*i+0),namelen);
	   if (2*i+1 < items) {
	       value = SvPV(ST(2*i+1),valuelen);
	   }
	   else {
	       value = "";
	       valuelen = 0;
	   }
	   var->shvcode = RXSHV_SET;
	   var->shvnext = &vars[i+1];
	   var->shvnamelen = namelen;
	   var->shvvaluelen = valuelen;
	   MAKERXSTRING(var->shvname, name, namelen);
	   MAKERXSTRING(var->shvvalue, value, valuelen);
	   if (trace)
	       fprintf(stderr, " %.*s='%.*s'",
		       var->shvname.strlength, var->shvname.strptr,
		       var->shvvalue.strlength, var->shvvalue.strptr);
       }
       if (trace)
	   fprintf(stderr, "\n");
       vars[n-1].shvnext = NULL;
       rc = RexxVariablePool(vars);
       if (trace)
	   fprintf(stderr, "  rc=%X\n", rc);
       RETVAL = (rc & ~RXSHV_NEWV) ? FALSE : TRUE;
   }
 OUTPUT:
    RETVAL

void
_fetch(name, ...)
	char *		name
 PPCODE:
   {
       int   i;
       ULONG rc;
       EXTEND(SP, items);
       needvars(items);
       if (trace)
	   fprintf(stderr, "REXXCALL::_fetch");
       for (i = 0; i < items; ++i) {
	   SHVBLOCK * var = &vars[i];
	   STRLEN     namelen;
	   name = SvPV(ST(i),namelen);
	   var->shvcode = RXSHV_FETCH;
	   var->shvnext = &vars[i+1];
	   var->shvnamelen = namelen;
	   var->shvvaluelen = 0;
	   MAKERXSTRING(var->shvname, name, namelen);
	   MAKERXSTRING(var->shvvalue, NULL, 0);
	   if (trace)
	       fprintf(stderr, " '%s'", name);
       }
       if (trace)
	   fprintf(stderr, "\n");
       vars[items-1].shvnext = NULL;
       rc = RexxVariablePool(vars);
       if (!(rc & ~RXSHV_NEWV)) {
	   for (i = 0; i < items; ++i) {
	       int namelen;
	       SHVBLOCK * var = &vars[i];
	       /* returned lengths appear to be swapped */
	       /* but beware of "future bug fixes" */
	       namelen = var->shvvalue.strlength; /* should be */
	       if (var->shvvaluelen < var->shvvalue.strlength)
		   namelen = var->shvvaluelen; /* is */
	       if (trace)
		   fprintf(stderr, "  %.*s='%.*s'\n",
			   var->shvname.strlength, var->shvname.strptr,
			   namelen, var->shvvalue.strptr);
	       if (var->shvret & RXSHV_NEWV || !var->shvvalue.strptr)
		   PUSHs(&PL_sv_undef);
	       else
		   PUSHs(sv_2mortal(newSVpv(var->shvvalue.strptr,
					    namelen)));
	   }
       } else {
	   if (trace)
	       fprintf(stderr, "  rc=%X\n", rc);
       }
   }

void
_next(stem)
	char *	stem
 PPCODE:
   {
       SHVBLOCK sv;
       BYTE     name[4096];
       ULONG    rc;
       int      len = strlen(stem), namelen, valuelen;
       if (trace)
	   fprintf(stderr, "REXXCALL::_next stem='%s'\n", stem);
       sv.shvcode = RXSHV_NEXTV;
       sv.shvnext = NULL;
       MAKERXSTRING(sv.shvvalue, NULL, 0);
       do {
	   sv.shvnamelen = sizeof name;
	   sv.shvvaluelen = 0;
	   MAKERXSTRING(sv.shvname, name, sizeof name);
	   if (sv.shvvalue.strptr) {
	       DosFreeMem(sv.shvvalue.strptr);
	       MAKERXSTRING(sv.shvvalue, NULL, 0);
	   }
	   rc = RexxVariablePool(&sv);
       } while (!rc && memcmp(stem, sv.shvname.strptr, len) != 0);
       if (!rc) {
	   EXTEND(SP, 2);
	   /* returned lengths appear to be swapped */
	   /* but beware of "future bug fixes" */
	   namelen = sv.shvname.strlength; /* should be */
	   if (sv.shvnamelen < sv.shvname.strlength)
	       namelen = sv.shvnamelen; /* is */
	   valuelen = sv.shvvalue.strlength; /* should be */
	   if (sv.shvvaluelen < sv.shvvalue.strlength)
	       valuelen = sv.shvvaluelen; /* is */
	   if (trace)
	       fprintf(stderr, "  %.*s='%.*s'\n",
		       namelen, sv.shvname.strptr,
		       valuelen, sv.shvvalue.strptr);
	   PUSHs(sv_2mortal(newSVpv(sv.shvname.strptr+len, namelen-len)));
	   if (sv.shvvalue.strptr) {
	       PUSHs(sv_2mortal(newSVpv(sv.shvvalue.strptr, valuelen)));
				DosFreeMem(sv.shvvalue.strptr);
	   } else	
	       PUSHs(&PL_sv_undef);
       } else if (rc != RXSHV_LVAR) {
	   die("Error %i when in _next", rc);
       } else {
	   if (trace)
	       fprintf(stderr, "  rc=%X\n", rc);
       }
   }

int
_drop(name,...)
	char *		name
 CODE:
   {
       int i;
       needvars(items);
       for (i = 0; i < items; ++i) {
	   SHVBLOCK * var = &vars[i];
	   STRLEN     namelen;
	   name = SvPV(ST(i),namelen);
	   var->shvcode = RXSHV_DROPV;
	   var->shvnext = &vars[i+1];
	   var->shvnamelen = namelen;
	   var->shvvaluelen = 0;
	   MAKERXSTRING(var->shvname, name, var->shvnamelen);
	   MAKERXSTRING(var->shvvalue, NULL, 0);
       }
       vars[items-1].shvnext = NULL;
       RETVAL = (RexxVariablePool(vars) & ~RXSHV_NEWV) ? FALSE : TRUE;
   }
 OUTPUT:
    RETVAL

int
_register(name)
	char *	name
 CODE:
    RETVAL = RexxRegisterFunctionExe(name, PERLCALL);
 OUTPUT:
    RETVAL

SV*
REXX_call(cv)
	SV *cv
  PROTOTYPE: &

SV*
REXX_eval(cmd)
	char *cmd

SV*
REXX_eval_with(cmd,name,cv)
	char *cmd
	char *name
	SV *cv
