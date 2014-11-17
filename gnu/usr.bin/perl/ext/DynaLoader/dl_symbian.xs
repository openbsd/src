/* dl_symbian.xs
 * 
 * Platform:	Symbian 7.0s
 * Author:	Jarkko Hietaniemi <jarkko.hietaniemi@nokia.com>
 * Copyright:	2004, Nokia
 * License:	Artistic/GPL
 *
 */

/*
 * In Symbian DLLs there is no name information, one can only access
 * the functions by their ordinals.  Perl, however, very much would like
 * to load functions by their names.  We fake this by having a special
 * setup function at the ordinal 1 (this is arranged by building the DLLs
 * in a special way).  The setup function builds a Perl hash mapping the
 * names to the ordinals, and the hash is then used by dlsym().
 *
 */

#include <e32base.h>
#include <eikdll.h>
#include <utf.h>

/* This is a useful pattern: first include the Symbian headers,
 * only after that the Perl ones.  Otherwise you will get a lot
 * trouble because of Symbian's New(), Copy(), etc definitions. */

#define DL_SYMBIAN_XS

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

START_EXTERN_C

void *dlopen(const char *filename, int flag);
void *dlsym(void *handle, const char *symbol);
int   dlclose(void *handle);
const char *dlerror(void);

extern void*  memset(void *s, int c, size_t n);
extern size_t strlen(const char *s);

END_EXTERN_C

#include "dlutils.c"

#define RTLD_LAZY   0x0001
#define RTLD_NOW    0x0002
#define RTLD_GLOBAL 0x0004

#ifndef NULL
#  define NULL 0
#endif

/* No need to pull in symbian_dll.cpp for this. */
#define symbian_get_vars() ((void*)Dll::Tls())

const TInt KPerlDllSetupFunction = 1;

typedef struct {
    RLibrary	handle;
    TInt	error;
    HV*		symbols;
} PerlSymbianLibHandle;

typedef void (*PerlSymbianLibInit)(void *);

void* dlopen(const char *filename, int flags) {
    TBuf16<KMaxFileName> utf16fn;
    const TUint8* utf8fn = (const TUint8*)filename;
    PerlSymbianLibHandle* h = NULL;
    TInt error;

    error =
        CnvUtfConverter::ConvertToUnicodeFromUtf8(utf16fn, TPtrC8(utf8fn));
    if (error == KErrNone) {
        h = new PerlSymbianLibHandle;
        if (h) {
            h->error   = KErrNone;
            h->symbols = (HV *)NULL;
        } else
            error = KErrNoMemory;
    }

    if (h && error == KErrNone) {
        error = (h->handle).Load(utf16fn);
        if (error == KErrNone) {
            TLibraryFunction init = (h->handle).Lookup(KPerlDllSetupFunction);
            ((PerlSymbianLibInit)init)(h);
        } else {
	    free(h);
            h = NULL;
        }
    }

    if (h)
        h->error = error;

    return h;
}

void* dlsym(void *handle, const char *symbol) {
    if (handle) {
        dTHX;
        PerlSymbianLibHandle* h = (PerlSymbianLibHandle*)handle;
        HV* symbols = h->symbols;
        if (symbols) {
            SV** svp = hv_fetch(symbols, symbol, strlen(symbol), FALSE);
            if (svp && *svp && SvIOK(*svp)) {
                IV ord = SvIV(*svp);
                if (ord > 0)
                    return (void*)((h->handle).Lookup(ord));
            }
        }
    }
    return NULL;
}

int dlclose(void *handle) {
    PerlSymbianLibHandle* h = (PerlSymbianLibHandle*)handle;
    if (h) {
        (h->handle).Close();
        if (h->symbols) {
            dTHX;
            hv_undef(h->symbols);
            h->symbols = NULL;
        }
        return 0;
    } else
        return 1;
}

const char* dlerror(void) {
    return 0;	/* Bad interface: assumes static data. */
}

static void
dl_private_init(pTHX)
{
    (void)dl_generic_private_init(aTHX);
}
 
MODULE = DynaLoader	PACKAGE = DynaLoader

PROTOTYPES:  ENABLE

BOOT:
    (void)dl_private_init(aTHX);


void
dl_load_file(filename, flags=0)
    char *	filename
    int		flags
  PREINIT:
    PerlSymbianLibHandle* h;
  CODE:
{
    ST(0) = sv_newmortal();
    h = (PerlSymbianLibHandle*)dlopen(filename, flags);
    if (h && h->error == KErrNone)
	sv_setiv(ST(0), PTR2IV(h));
    else
	PerlIO_printf(Perl_debug_log, "(dl_load_file %s %d)",
                      filename, h ? h->error : -1);
}


int
dl_unload_file(libhandle)
    void *	libhandle
  CODE:
    RETVAL = (dlclose(libhandle) == 0 ? 1 : 0);
  OUTPUT:
    RETVAL


void
dl_find_symbol(libhandle, symbolname)
    void *	libhandle
    char *	symbolname
    PREINIT:
    void *sym;
    CODE:
    PerlSymbianLibHandle* h = (PerlSymbianLibHandle*)libhandle;
    sym = dlsym(libhandle, symbolname);
    ST(0) = sv_newmortal();
    if (sym)
       sv_setiv(ST(0), PTR2IV(sym));
    else
       PerlIO_printf(Perl_debug_log, "(dl_find_symbol %s %d)",
                     symbolname, h ? h->error : -1);


void
dl_undef_symbols()
    CODE:



# These functions should not need changing on any platform:

void
dl_install_xsub(perl_name, symref, filename="$Package")
    char *		perl_name
    void *		symref 
    const char *	filename
    CODE:
    ST(0) = sv_2mortal(newRV((SV*)newXS_flags(perl_name,
					      (void(*)(pTHX_ CV *))symref,
					      filename, NULL,
					      XS_DYNAMIC_FILENAME)));


char *
dl_error()
    CODE:
    dMY_CXT;
    RETVAL = dl_last_error;
    OUTPUT:
    RETVAL

#if defined(USE_ITHREADS)

void
CLONE(...)
    CODE:
    MY_CXT_CLONE;

    PERL_UNUSED_VAR(items);

    /* MY_CXT_CLONE just does a memcpy on the whole structure, so to avoid
     * using Perl variables that belong to another thread, we create our 
     * own for this thread.
     */
    MY_CXT.x_dl_last_error = newSVpvn("", 0);

#endif

# end.
