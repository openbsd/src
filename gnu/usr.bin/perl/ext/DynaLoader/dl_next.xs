/* dl_next.xs
 * 
 * Platform:	NeXT NS 3.2
 * Author:	Anno Siegel (siegel@zrz.TU-Berlin.DE)
 * Based on:	dl_dlopen.xs by Paul Marquess
 * Created:	Aug 15th, 1994
 *
 */

/*
 *  And Gandalf said: 'Many folk like to know beforehand what is to
 *  be set on the table; but those who have laboured to prepare the
 *  feast like to keep their secret; for wonder makes the words of
 *  praise louder.'
 *
 *     [p.970 of _The Lord of the Rings_, VI/v: "The Steward and the King"]
 */

/* Porting notes:

dl_next.xs is itself a port from dl_dlopen.xs by Paul Marquess.  It
should not be used as a base for further ports though it may be used
as an example for how dl_dlopen.xs can be ported to other platforms.

The method used here is just to supply the sun style dlopen etc.
functions in terms of NeXTs rld_*.  The xs code proper is unchanged
from Paul's original.

The port could use some streamlining.  For one, error handling could
be simplified.

Anno Siegel

*/

#if NS_TARGET_MAJOR >= 4
#else
/* include these before perl headers */
#include <mach-o/rld.h>
#include <streams/streams.h>
#endif

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#define DL_LOADONCEONLY

typedef struct {
    AV *	x_resolve_using;
} my_cxtx_t;		/* this *must* be named my_cxtx_t */

#define DL_CXT_EXTRA	/* ask for dl_cxtx to be defined in dlutils.c */
#include "dlutils.c"	/* SaveError() etc	*/

#define dl_resolve_using	(dl_cxtx.x_resolve_using)

static char *dlerror()
{
    dTHX;
    dMY_CXT;
    return dl_last_error;
}

int dlclose(handle) /* stub only */
void *handle;
{
    return 0;
}

#if NS_TARGET_MAJOR >= 4
#import <mach-o/dyld.h>

enum dyldErrorSource
{
    OFImage,
};

static void TranslateError
    (const char *path, enum dyldErrorSource type, int number)
{
    dTHX;
    dMY_CXT;
    char *error;
    unsigned int index;
    static char *OFIErrorStrings[] =
    {
	"%s(%d): Object Image Load Failure\n",
	"%s(%d): Object Image Load Success\n",
	"%s(%d): Not a recognisable object file\n",
	"%s(%d): No valid architecture\n",
	"%s(%d): Object image has an invalid format\n",
	"%s(%d): Invalid access (permissions?)\n",
	"%s(%d): Unknown error code from NSCreateObjectFileImageFromFile\n",
    };
#define NUM_OFI_ERRORS (sizeof(OFIErrorStrings) / sizeof(OFIErrorStrings[0]))

    switch (type)
    {
    case OFImage:
	index = number;
	if (index > NUM_OFI_ERRORS - 1)
	    index = NUM_OFI_ERRORS - 1;
	error = Perl_form_nocontext(OFIErrorStrings[index], path, number);
	break;

    default:
	error = Perl_form_nocontext("%s(%d): Totally unknown error type %d\n",
		     path, number, type);
	break;
    }
    Safefree(dl_last_error);
    dl_last_error = savepv(error);
}

static char *dlopen(char *path, int mode /* mode is ignored */)
{
    int dyld_result;
    NSObjectFileImage ofile;
    NSModule handle = NULL;

    dyld_result = NSCreateObjectFileImageFromFile(path, &ofile);
    if (dyld_result != NSObjectFileImageSuccess)
	TranslateError(path, OFImage, dyld_result);
    else
    {
    	// NSLinkModule will cause the run to abort on any link error's
	// not very friendly but the error recovery functionality is limited.
	handle = NSLinkModule(ofile, path, TRUE);
    }
    
    return handle;
}

void *
dlsym(handle, symbol)
void *handle;
char *symbol;
{
    void *addr;

    if (NSIsSymbolNameDefined(symbol))
	addr = NSAddressOfSymbol(NSLookupAndBindSymbol(symbol));
    else
    	addr = NULL;

    return addr;
}

#else /* NS_TARGET_MAJOR <= 3 */

static NXStream *OpenError(void)
{
    return NXOpenMemory( (char *) 0, 0, NX_WRITEONLY);
}

static void TransferError(NXStream *s)
{
    char *buffer;
    int len, maxlen;
    dTHX;
    dMY_CXT;

    if ( dl_last_error ) {
        Safefree(dl_last_error);
    }
    NXGetMemoryBuffer(s, &buffer, &len, &maxlen);
    Newx(dl_last_error, len, char);
    strcpy(dl_last_error, buffer);
}

static void CloseError(NXStream *s)
{
    if ( s ) {
      NXCloseMemory( s, NX_FREEBUFFER);
    }
}

static char *dlopen(char *path, int mode /* mode is ignored */)
{
    int rld_success;
    NXStream *nxerr;
    I32 i, psize;
    char *result;
    char **p;
    STRLEN n_a;
    dTHX;
    dMY_CXT;
	
    /* Do not load what is already loaded into this process */
    if (hv_fetch(dl_loaded_files, path, strlen(path), 0))
	return path;

    nxerr = OpenError();
    psize = AvFILL(dl_resolve_using) + 3;
    p = (char **) safemalloc(psize * sizeof(char*));
    p[0] = path;
    for(i=1; i<psize-1; i++) {
	p[i] = SvPVx(*av_fetch(dl_resolve_using, i-1, TRUE), n_a);
    }
    p[psize-1] = 0;
    rld_success = rld_load(nxerr, (struct mach_header **)0, p,
			    (const char *) 0);
    safefree((char*) p);
    if (rld_success) {
	result = path;
	/* prevent multiple loads of same file into same process */
	hv_store(dl_loaded_files, path, strlen(path), &PL_sv_yes, 0);
    } else {
	TransferError(nxerr);
	result = (char*) 0;
    }
    CloseError(nxerr);
    return result;
}

void *
dlsym(handle, symbol)
void *handle;
char *symbol;
{
    NXStream	*nxerr = OpenError();
    unsigned long	symref = 0;

    if (!rld_lookup(nxerr, Perl_form_nocontext("_%s", symbol), &symref))
	TransferError(nxerr);
    CloseError(nxerr);
    return (void*) symref;
}

#endif /* NS_TARGET_MAJOR >= 4 */


/* ----- code from dl_dlopen.xs below here ----- */


static void
dl_private_init(pTHX)
{
    (void)dl_generic_private_init(aTHX);
    {
	dMY_CXT;
	dl_resolve_using = get_av("DynaLoader::dl_resolve_using", GV_ADDMULTI);
    }
}
 
MODULE = DynaLoader     PACKAGE = DynaLoader

BOOT:
    (void)dl_private_init(aTHX);



void
dl_load_file(filename, flags=0)
    char *	filename
    int		flags
    PREINIT:
    int mode = 1;
    void *retv;
    CODE:
    DLDEBUG(1,PerlIO_printf(Perl_debug_log, "dl_load_file(%s,%x):\n", filename,flags));
    if (flags & 0x01)
	Perl_warn(aTHX_ "Can't make loaded symbols global on this platform while loading %s",filename);
    retv = dlopen(filename, mode) ;
    DLDEBUG(2,PerlIO_printf(Perl_debug_log, " libref=%x\n", retv));
    ST(0) = sv_newmortal() ;
    if (retv == NULL)
	SaveError(aTHX_ "%s",dlerror()) ;
    else
	sv_setiv( ST(0), PTR2IV(retv) );


void
dl_find_symbol(libhandle, symbolname)
    void *		libhandle
    char *		symbolname
    PREINIT:
    void *retv;
    CODE:
#if NS_TARGET_MAJOR >= 4
    symbolname = Perl_form_nocontext("_%s", symbolname);
#endif
    DLDEBUG(2, PerlIO_printf(Perl_debug_log,
			     "dl_find_symbol(handle=%lx, symbol=%s)\n",
			     (unsigned long) libhandle, symbolname));
    retv = dlsym(libhandle, symbolname);
    DLDEBUG(2, PerlIO_printf(Perl_debug_log,
			     "  symbolref = %lx\n", (unsigned long) retv));
    ST(0) = sv_newmortal() ;
    if (retv == NULL)
	SaveError(aTHX_ "%s",dlerror()) ;
    else
	sv_setiv( ST(0), PTR2IV(retv) );


void
dl_undef_symbols()
    CODE:



# These functions should not need changing on any platform:

void
dl_install_xsub(perl_name, symref, filename="$Package")
    char *	perl_name
    void *	symref 
    const char *	filename
    CODE:
    DLDEBUG(2,PerlIO_printf(Perl_debug_log, "dl_install_xsub(name=%s, symref=%x)\n",
	    perl_name, symref));
    ST(0) = sv_2mortal(newRV((SV*)newXS_flags(perl_name,
					      (void(*)(pTHX_ CV *))symref,
					      filename, NULL,
					      XS_DYNAMIC_FILENAME)));


char *
dl_error()
    CODE:
    dMY_CXT;
    RETVAL = dl_last_error ;
    OUTPUT:
    RETVAL

#if defined(USE_ITHREADS)

void
CLONE(...)
    CODE:
    MY_CXT_CLONE;

    /* MY_CXT_CLONE just does a memcpy on the whole structure, so to avoid
     * using Perl variables that belong to another thread, we create our 
     * own for this thread.
     */
    MY_CXT.x_dl_last_error = newSVpvn("", 0);
    dl_resolve_using = get_av("DynaLoader::dl_resolve_using", GV_ADDMULTI);

#endif

# end.
