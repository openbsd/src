/* dl_os2.xs
 * 
 * Platform:	OS/2.
 * Author:	Andreas Kaiser (ak@ananke.s.bawue.de)
 * Created:	08th December 1994
 */

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#define INCL_BASE
#include <os2.h>

#include "dlutils.c"	/* SaveError() etc	*/

static ULONG retcode;

static void *
dlopen(char *path, int mode)
{
	HMODULE handle;
	char tmp[260], *beg, *dot;
	char fail[300];
	ULONG rc;

	if ((rc = DosLoadModule(fail, sizeof fail, path, &handle)) == 0)
		return (void *)handle;

	retcode = rc;

	/* Not found. Check for non-FAT name and try truncated name. */
	/* Don't know if this helps though... */
	for (beg = dot = path + strlen(path);
	     beg > path && !strchr(":/\\", *(beg-1));
	     beg--)
		if (*beg == '.')
			dot = beg;
	if (dot - beg > 8) {
		int n = beg+8-path;
		memmove(tmp, path, n);
		memmove(tmp+n, dot, strlen(dot)+1);
		if (DosLoadModule(fail, sizeof fail, tmp, &handle) == 0)
			return (void *)handle;
	}

	return NULL;
}

static void *
dlsym(void *handle, char *symbol)
{
	ULONG rc, type;
	PFN addr;

	rc = DosQueryProcAddr((HMODULE)handle, 0, symbol, &addr);
	if (rc == 0) {
		rc = DosQueryProcType((HMODULE)handle, 0, symbol, &type);
		if (rc == 0 && type == PT_32BIT)
			return (void *)addr;
		rc = ERROR_CALL_NOT_IMPLEMENTED;
	}
	retcode = rc;
	return NULL;
}

static char *
dlerror(void)
{
	static char buf[300];
	ULONG len;

	if (retcode == 0)
		return NULL;
	if (DosGetMessage(NULL, 0, buf, sizeof buf - 1, retcode, "OSO001.MSG", &len))
		sprintf(buf, "OS/2 system error code %d", retcode);
	else
		buf[len] = '\0';
	retcode = 0;
	return buf;
}


static void
dl_private_init()
{
    (void)dl_generic_private_init();
}

static char *
mod2fname(sv)
     SV   *sv;
{
    static char fname[9];
    int pos = 7;
    int len;
    AV  *av;
    SV  *svp;
    char *s;

    if (!SvROK(sv)) croak("Not a reference given to mod2fname");
    sv = SvRV(sv);
    if (SvTYPE(sv) != SVt_PVAV) 
      croak("Not array reference given to mod2fname");
    if (av_len((AV*)sv) < 0) 
      croak("Empty array reference given to mod2fname");
    s = SvPV(*av_fetch((AV*)sv, av_len((AV*)sv), FALSE), na);
    strncpy(fname, s, 8);
    if ((len=strlen(s)) < 7) pos = len;
    fname[pos] = '_';
    fname[pos + 1] = '\0';
    return (char *)fname;
}

MODULE = DynaLoader	PACKAGE = DynaLoader

BOOT:
    (void)dl_private_init();


void *
dl_load_file(filename)
    char *		filename
    CODE:
    int mode = 1;     /* Solaris 1 */
#ifdef RTLD_LAZY
    mode = RTLD_LAZY; /* Solaris 2 */
#endif
    DLDEBUG(1,fprintf(stderr,"dl_load_file(%s):\n", filename));
    RETVAL = dlopen(filename, mode) ;
    DLDEBUG(2,fprintf(stderr," libref=%x\n", RETVAL));
    ST(0) = sv_newmortal() ;
    if (RETVAL == NULL)
	SaveError("%s",dlerror()) ;
    else
	sv_setiv( ST(0), (IV)RETVAL);


void *
dl_find_symbol(libhandle, symbolname)
    void *	libhandle
    char *	symbolname
    CODE:
#ifdef DLSYM_NEEDS_UNDERSCORE
    char symbolname_buf[1024];
    symbolname = dl_add_underscore(symbolname, symbolname_buf);
#endif
    DLDEBUG(2,fprintf(stderr,"dl_find_symbol(handle=%x, symbol=%s)\n",
	libhandle, symbolname));
    RETVAL = dlsym(libhandle, symbolname);
    DLDEBUG(2,fprintf(stderr,"  symbolref = %x\n", RETVAL));
    ST(0) = sv_newmortal() ;
    if (RETVAL == NULL)
	SaveError("%s",dlerror()) ;
    else
	sv_setiv( ST(0), (IV)RETVAL);


void
dl_undef_symbols()
    PPCODE:

char *
mod2fname(sv)
     SV   *sv;


# These functions should not need changing on any platform:

void
dl_install_xsub(perl_name, symref, filename="$Package")
    char *		perl_name
    void *		symref 
    char *		filename
    CODE:
    DLDEBUG(2,fprintf(stderr,"dl_install_xsub(name=%s, symref=%x)\n",
		perl_name, symref));
    ST(0)=sv_2mortal(newRV((SV*)newXS(perl_name, (void(*)())symref, filename)));


char *
dl_error()
    CODE:
    RETVAL = LastError ;
    OUTPUT:
    RETVAL

# end.
