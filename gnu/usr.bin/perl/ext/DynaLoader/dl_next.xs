/* dl_next.xs
 * 
 * Platform:	NeXT NS 3.2
 * Author:	Anno Siegel (siegel@zrz.TU-Berlin.DE)
 * Based on:	dl_dlopen.xs by Paul Marquess
 * Created:	Aug 15th, 1994
 *
 */

/*
    And Gandalf said: 'Many folk like to know beforehand what is to
    be set on the table; but those who have laboured to prepare the
    feast like to keep their secret; for wonder makes the words of
    praise louder.'
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

/* include these before perl headers */
#include <mach-o/rld.h>
#include <streams/streams.h>

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#define DL_LOADONCEONLY

#include "dlutils.c"	/* SaveError() etc	*/


static char * dl_last_error = (char *) 0;
static AV *dl_resolve_using = Nullav;

NXStream *
OpenError()
{
    return NXOpenMemory( (char *) 0, 0, NX_WRITEONLY);
}

void
TransferError( s)
NXStream *s;
{
    char *buffer;
    int len, maxlen;

    if ( dl_last_error ) {
        safefree(dl_last_error);
    }
    NXGetMemoryBuffer(s, &buffer, &len, &maxlen);
    dl_last_error = safemalloc(len);
    strcpy(dl_last_error, buffer);
}

void
CloseError( s)
NXStream *s;
{
    if ( s ) {
      NXCloseMemory( s, NX_FREEBUFFER);
    }
}

char *dlerror()
{
    return dl_last_error;
}

char *
dlopen(path, mode)
char * path;
int mode; /* mode is ignored */
{
    int rld_success;
    NXStream *nxerr;
    I32 i, psize;
    char *result;
    char **p;
	
    /* Do not load what is already loaded into this process */
    if (hv_fetch(dl_loaded_files, path, strlen(path), 0))
	return path;

    nxerr = OpenError();
    psize = AvFILL(dl_resolve_using) + 3;
    p = (char **) safemalloc(psize * sizeof(char*));
    p[0] = path;
    for(i=1; i<psize-1; i++) {
	p[i] = SvPVx(*av_fetch(dl_resolve_using, i-1, TRUE), na);
    }
    p[psize-1] = 0;
    rld_success = rld_load(nxerr, (struct mach_header **)0, p,
			    (const char *) 0);
    safefree((char*) p);
    if (rld_success) {
	result = path;
	/* prevent multiple loads of same file into same process */
	hv_store(dl_loaded_files, path, strlen(path), &sv_yes, 0);
    } else {
	TransferError(nxerr);
	result = (char*) 0;
    }
    CloseError(nxerr);
    return result;
}

int
dlclose(handle) /* stub only */
void *handle;
{
    return 0;
}

void *
dlsym(handle, symbol)
void *handle;
char *symbol;
{
    NXStream	*nxerr = OpenError();
    char	symbuf[1024];
    unsigned long	symref = 0;

    sprintf(symbuf, "_%s", symbol);
    if (!rld_lookup(nxerr, symbuf, &symref)) {
	TransferError(nxerr);
    }
    CloseError(nxerr);
    return (void*) symref;
}


/* ----- code from dl_dlopen.xs below here ----- */


static void
dl_private_init()
{
    (void)dl_generic_private_init();
    dl_resolve_using = perl_get_av("DynaLoader::dl_resolve_using", 0x4);
}
 
MODULE = DynaLoader     PACKAGE = DynaLoader

BOOT:
    (void)dl_private_init();



void *
dl_load_file(filename)
    char *	filename
    CODE:
    int mode = 1;
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
    void *		libhandle
    char *		symbolname
    CODE:
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



# These functions should not need changing on any platform:

void
dl_install_xsub(perl_name, symref, filename="$Package")
    char *	perl_name
    void *	symref 
    char *	filename
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
