/*
 * Author: Jeff Okamoto (okamoto@corp.hp.com)
 * Version: 2.1, 1995/1/25
 */

#ifdef __hp9000s300
#define magic hpux_magic
#define MAGIC HPUX_MAGIC
#endif

#include <dl.h>
#ifdef __hp9000s300
#undef magic
#undef MAGIC
#endif

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"


#include "dlutils.c"	/* for SaveError() etc */

static AV *dl_resolve_using = Nullav;


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
    char *		filename
    CODE:
    shl_t obj = NULL;
    int	i, max, bind_type;

    if (dl_nonlazy)
	bind_type = BIND_IMMEDIATE;
    else
	bind_type = BIND_DEFERRED;

    max = AvFILL(dl_resolve_using);
    for (i = 0; i <= max; i++) {
	char *sym = SvPVX(*av_fetch(dl_resolve_using, i, 0));
	DLDEBUG(1,fprintf(stderr, "dl_load_file(%s) (dependent)\n", sym));
	obj = shl_load(sym, bind_type | BIND_NOSTART, 0L);
	if (obj == NULL) {
	    goto end;
	}
    }

    DLDEBUG(1,fprintf(stderr,"dl_load_file(%s): ", filename));
    obj = shl_load(filename, bind_type | BIND_NOSTART, 0L);

    DLDEBUG(2,fprintf(stderr," libref=%x\n", obj));
end:
    ST(0) = sv_newmortal() ;
    if (obj == NULL)
        SaveError("%s",Strerror(errno));
    else
        sv_setiv( ST(0), (IV)obj);


void *
dl_find_symbol(libhandle, symbolname)
    void *	libhandle
    char *	symbolname
    CODE:
    shl_t obj = (shl_t) libhandle;
    void *symaddr = NULL;
    int status;
#ifdef __hp9000s300
    char symbolname_buf[MAXPATHLEN];
    symbolname = dl_add_underscore(symbolname, symbolname_buf);
#endif
    DLDEBUG(2,fprintf(stderr,"dl_find_symbol(handle=%x, symbol=%s)\n",
		libhandle, symbolname));
    ST(0) = sv_newmortal() ;
    errno = 0;

    status = shl_findsym(&obj, symbolname, TYPE_PROCEDURE, &symaddr);
    DLDEBUG(2,fprintf(stderr,"  symbolref(PROCEDURE) = %x\n", symaddr));

    if (status == -1 && errno == 0) {	/* try TYPE_DATA instead */
	status = shl_findsym(&obj, symbolname, TYPE_DATA, &symaddr);
	DLDEBUG(2,fprintf(stderr,"  symbolref(DATA) = %x\n", symaddr));
    }

    if (status == -1) {
	SaveError("%s",(errno) ? Strerror(errno) : "Symbol not found") ;
    } else {
	sv_setiv( ST(0), (IV)symaddr);
    }


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
