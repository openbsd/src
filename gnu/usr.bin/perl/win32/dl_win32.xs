/* dl_win32.xs
 * 
 * Platform:	Win32 (Windows NT/Windows 95)
 * Author:	Wei-Yuen Tan (wyt@hip.com)
 * Created:	A warm day in June, 1995
 *
 * Modified:
 *    August 23rd 1995 - rewritten after losing everything when I
 *                       wiped off my NT partition (eek!)
 */

/* Porting notes:

I merely took Paul's dl_dlopen.xs, took out extraneous stuff and
replaced the appropriate SunOS calls with the corresponding Win32
calls.

*/

#define WIN32_LEAN_AND_MEAN
#ifdef __GNUC__
#define Win32_Winsock
#endif
#include <windows.h>
#include <string.h>

#include "EXTERN.h"
#include "perl.h"
#include "win32.h"

#ifdef PERL_OBJECT
#define NO_XSLOCKS
#endif  /* PERL_OBJECT */

#include "XSUB.h"

static SV *error_sv;

static char *
OS_Error_String(CPERLarg)
{
 DWORD err = GetLastError();
 STRLEN len;
 if (!error_sv)
  error_sv = newSVpv("",0);
 win32_str_os_error(error_sv,err);
 return SvPV(error_sv,len);
}

#include "dlutils.c"	/* SaveError() etc	*/

static void
dl_private_init(CPERLarg)
{
    (void)dl_generic_private_init(PERL_OBJECT_THIS);
}

/* 
    This function assumes the list staticlinkmodules
    will be formed from package names with '::' replaced
    with '/'. Thus Win32::OLE is in the list as Win32/OLE
*/
static int
dl_static_linked(char *filename)
{
    char **p;
    char* ptr;
    static char subStr[] = "/auto/";
    char szBuffer[MAX_PATH];

    /* change all the '\\' to '/' */
    strcpy(szBuffer, filename);
    for(ptr = szBuffer; ptr = strchr(ptr, '\\'); ++ptr)
	*ptr = '/';

    /* delete the file name */
    ptr = strrchr(szBuffer, '/');
    if(ptr != NULL)
	*ptr = '\0';

    /* remove leading lib path */
    ptr = strstr(szBuffer, subStr);
    if(ptr != NULL)
	ptr += sizeof(subStr)-1;
    else
	ptr = szBuffer;

    for (p = staticlinkmodules; *p;p++) {
	if (strstr(ptr, *p)) return 1;
    };
    return 0;
}

MODULE = DynaLoader	PACKAGE = DynaLoader

BOOT:
    (void)dl_private_init(PERL_OBJECT_THIS);

void *
dl_load_file(filename,flags=0)
    char *		filename
    int			flags
    PREINIT:
    CODE:
    DLDEBUG(1,PerlIO_printf(PerlIO_stderr(),"dl_load_file(%s):\n", filename));
    if (dl_static_linked(filename) == 0)
	RETVAL = (void*) LoadLibraryEx(filename, NULL, LOAD_WITH_ALTERED_SEARCH_PATH ) ;
    else
	RETVAL = (void*) GetModuleHandle(NULL);
    DLDEBUG(2,PerlIO_printf(PerlIO_stderr()," libref=%x\n", RETVAL));
    ST(0) = sv_newmortal() ;
    if (RETVAL == NULL)
	SaveError(PERL_OBJECT_THIS_ "load_file:%s",
		  OS_Error_String(PERL_OBJECT_THIS)) ;
    else
	sv_setiv( ST(0), (IV)RETVAL);


void *
dl_find_symbol(libhandle, symbolname)
    void *	libhandle
    char *	symbolname
    CODE:
    DLDEBUG(2,PerlIO_printf(PerlIO_stderr(),"dl_find_symbol(handle=%x, symbol=%s)\n",
		      libhandle, symbolname));
    RETVAL = (void*) GetProcAddress((HINSTANCE) libhandle, symbolname);
    DLDEBUG(2,PerlIO_printf(PerlIO_stderr(),"  symbolref = %x\n", RETVAL));
    ST(0) = sv_newmortal() ;
    if (RETVAL == NULL)
	SaveError(PERL_OBJECT_THIS_ "find_symbol:%s",
		  OS_Error_String(PERL_OBJECT_THIS)) ;
    else
	sv_setiv( ST(0), (IV)RETVAL);


void
dl_undef_symbols()
    PPCODE:



# These functions should not need changing on any platform:

void
dl_install_xsub(perl_name, symref, filename="$Package")
    char *		perl_name
    void *		symref 
    char *		filename
    CODE:
    DLDEBUG(2,PerlIO_printf(PerlIO_stderr(),"dl_install_xsub(name=%s, symref=%x)\n",
		      perl_name, symref));
    ST(0)=sv_2mortal(newRV((SV*)newXS(perl_name, (void(*)(CV* _CPERLarg))symref, filename)));


char *
dl_error()
    CODE:
    RETVAL = LastError ;
    OUTPUT:
    RETVAL

# end.
