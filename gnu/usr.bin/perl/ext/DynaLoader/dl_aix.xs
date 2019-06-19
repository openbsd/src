/* dl_aix.xs
 *
 * Written: 8/31/94 by Wayne Scott (wscott@ichips.intel.com)
 *
 *  All I did was take Jens-Uwe Mager's libdl emulation library for
 *  AIX and merged it with the dl_dlopen.xs file to create a dynamic library
 *  package that works for AIX.
 *
 *  I did change all malloc's, free's, strdup's, calloc's to use the perl
 *  equilvant.  I also removed some stuff we will not need.  Call fini()
 *  on startup...   It can probably be trimmed more.
 */

#define PERLIO_NOT_STDIO 0
#define PERL_EXT
#define PERL_IN_DL_AIX_XS

/*
 * On AIX 4.3 and above the emulation layer is not needed any more, and
 * indeed if perl uses its emulation and perl is linked into apache
 * which is supposed to use the native dlopen conflicts arise.
 * Jens-Uwe Mager jum@helios.de
 */
#ifdef USE_NATIVE_DLOPEN

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include <dlfcn.h>

#include "dlutils.c"	/* SaveError() etc	*/

#else

/*
 * @(#)dlfcn.c	1.5 revision of 93/02/14  20:14:17
 * This is an unpublished work copyright (c) 1992 Helios Software GmbH
 * 3000 Hannover 1, Germany
 */
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

/* When building as a 64-bit binary on AIX, define this to get the
 * correct structure definitions.  Also determines the field-name
 * macros and gates some logic in readEntries().  -- Steven N. Hirsch
 * <hirschs@btv.ibm.com> */
#ifdef USE_64_BIT_ALL
#   define __XCOFF64__
#   define __XCOFF32__
#endif

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ldr.h>
#include <a.out.h>
#undef FREAD
#undef FWRITE
#include <ldfcn.h>

#ifdef USE_64_BIT_ALL
#   define AIX_SCNHDR SCNHDR_64
#   define AIX_LDHDR LDHDR_64
#   define AIX_LDSYM LDSYM_64
#   define AIX_LDHDRSZ LDHDRSZ_64
#else
#   define AIX_SCNHDR SCNHDR
#   define AIX_LDHDR LDHDR
#   define AIX_LDSYM LDSYM
#   define AIX_LDHDRSZ LDHDRSZ
#endif

/* When using Perl extensions written in C++ the longer versions
 * of load() and unload() from libC and libC_r need to be used,
 * otherwise statics in the extensions won't get initialized right.
 * -- Stephanie Beals <bealzy@us.ibm.com> */

/* Older AIX C compilers cannot deal with C++ double-slash comments in
   the ibmcxx and/or xlC includes.  Since we only need a single file,
   be more fine-grained about what's included <hirschs@btv.ibm.com> */

#ifdef USE_libC /* The define comes, when it comes, from hints/aix.pl. */
#   define LOAD   loadAndInit
#   define UNLOAD terminateAndUnload
#   if defined(USE_vacpp_load_h)
#       include "/usr/vacpp/include/load.h"
#   elif defined(USE_ibmcxx_load_h)
#       include "/usr/ibmcxx/include/load.h"
#   elif defined(USE_xlC_load_h)
#       include "/usr/lpp/xlC/include/load.h"
#   elif defined(USE_load_h)
#       include "/usr/include/load.h"
#   endif
#else
#   define LOAD   load
#   define UNLOAD unload
#endif

/*
 * AIX 4.3 does remove some useful definitions from ldfcn.h. Define
 * these here to compensate for that lossage.
 */
#ifndef BEGINNING
# define BEGINNING SEEK_SET
#endif
#ifndef FSEEK
# define FSEEK(ldptr,o,p)	fseek(IOPTR(ldptr),(p==BEGINNING)?(OFFSET(ldptr) +o):o,p)
#endif
#ifndef FREAD
# define FREAD(p,s,n,ldptr)	fread(p,s,n,IOPTR(ldptr))
#endif

#ifndef RTLD_LAZY
# define RTLD_LAZY 0
#endif
#ifndef RTLD_GLOBAL
# define RTLD_GLOBAL 0
#endif

/*
 * We simulate dlopen() et al. through a call to load. Because AIX has
 * no call to find an exported symbol we read the loader section of the
 * loaded module and build a list of exported symbols and their virtual
 * address.
 */

typedef struct {
	char		*name;		/* the symbols's name */
	void		*addr;		/* its relocated virtual address */
} Export, *ExportPtr;

/*
 * The void * handle returned from dlopen is actually a ModulePtr.
 */
typedef struct Module {
	struct Module	*next;
	char		*name;		/* module name for refcounting */
	int		refCnt;		/* the number of references */
	void		*entry;		/* entry point from load */
	int		nExports;	/* the number of exports found */
	ExportPtr	exports;	/* the array of exports */
} Module, *ModulePtr;

typedef struct {
    /*
     * We keep a list of all loaded modules to be able to reference count
     * duplicate dlopen's.
     */
    ModulePtr	x_modList;

    /*
     * The last error from one of the dl* routines is kept in static
     * variables here. Each error is returned only once to the caller.
     */
    char	x_errbuf[BUFSIZ];
    int		x_errvalid;
    void *	x_mainModule;
} my_cxtx_t;		/* this *must* be named my_cxtx_t */

#define DL_CXT_EXTRA	/* ask for dl_cxtx to be defined in dlutils.c */
#include "dlutils.c"	/* SaveError() etc	*/

#define dl_modList	(dl_cxtx.x_modList)
#define dl_errbuf	(dl_cxtx.x_errbuf)
#define dl_errvalid	(dl_cxtx.x_errvalid)
#define dl_mainModule	(dl_cxtx.x_mainModule)

static void caterr(char *);
static int readExports(ModulePtr);
static void *findMain(void);

/* these statics are ok because they're constants */
static char *strerror_failed   = "(strerror failed)";
static char *strerror_r_failed = "(strerror_r failed)";

char *strerrorcat(char *str, int err) {
    int strsiz = strlen(str);
    int msgsiz;
    char *msg;

    dTHX;

    if ((msg = strerror(err)) == 0)
      msg = strerror_failed;
    msgsiz = strlen(msg);		/* Note msg = buf and free() above. */
    if (strsiz + msgsiz < BUFSIZ)	/* Do not move this after #endif. */
      strcat(str, msg);

    return str;
}

char *strerrorcpy(char *str, int err) {
    int msgsiz;
    char *msg;

    dTHX;

    if ((msg = strerror(err)) == 0)
      msg = strerror_failed;
    msgsiz = strlen(msg);	/* Note msg = buf and free() above. */
    if (msgsiz < BUFSIZ)	/* Do not move this after #endif. */
      strcpy(str, msg);

    return str;
}
  
/* ARGSUSED */
void *dlopen(char *path, int mode)
{
	dTHX;
	dMY_CXT;
	ModulePtr mp;

	/*
	 * Upon the first call register a terminate handler that will
	 * close all libraries.
	 */
	if (dl_mainModule == NULL) {
		if ((dl_mainModule = findMain()) == NULL)
			return NULL;
	}
	/*
	 * Scan the list of modules if have the module already loaded.
	 */
	for (mp = dl_modList; mp; mp = mp->next)
		if (strEQ(mp->name, path)) {
			mp->refCnt++;
			return mp;
		}
	Newxz(mp,1,Module);
	if (mp == NULL) {
		dl_errvalid++;
		strcpy(dl_errbuf, "Newz: ");
		strerrorcat(dl_errbuf, errno);
		return NULL;
	}
	
	if ((mp->name = savepv(path)) == NULL) {
		dl_errvalid++;
		strcpy(dl_errbuf, "savepv: ");
		strerrorcat(dl_errbuf, errno);
		safefree(mp);
		return NULL;
	}

	/*
	 * load should be declared load(const char *...). Thus we
	 * cast the path to a normal char *. Ugly.
	 */
	if ((mp->entry = (void *)LOAD((char *)path,
#ifdef L_LIBPATH_EXEC
				      L_LIBPATH_EXEC |
#endif
				      L_NOAUTODEFER,
				      NULL)) == NULL) {
	        int saverrno = errno;
		
		safefree(mp->name);
		safefree(mp);
		dl_errvalid++;
		strcpy(dl_errbuf, "dlopen: ");
		strcat(dl_errbuf, path);
		strcat(dl_errbuf, ": ");
		/*
		 * If AIX says the file is not executable, the error
		 * can be further described by querying the loader about
		 * the last error.
		 */
		if (saverrno == ENOEXEC) {
			char *moreinfo[BUFSIZ/sizeof(char *)];
			if (loadquery(L_GETMESSAGES, moreinfo, sizeof(moreinfo)) == -1)
				strerrorcpy(dl_errbuf, saverrno);
			else {
				char **p;
				for (p = moreinfo; *p; p++)
					caterr(*p);
			}
		} else
			strerrorcat(dl_errbuf, saverrno);
		return NULL;
	}
	mp->refCnt = 1;
	mp->next = dl_modList;
	dl_modList = mp;
	/*
	 * Assume anonymous exports come from the module this dlopen
	 * is linked into, that holds true as long as dlopen and all
	 * of the perl core are in the same shared object. Also bind
	 * against the main part, in the case a perl is not the main
	 * part, e.g mod_perl as DSO in Apache so perl modules can
	 * also reference Apache symbols.
	 */
	if (loadbind(0, (void *)dlopen, mp->entry) == -1 ||
	    loadbind(0, dl_mainModule, mp->entry)) {
	        int saverrno = errno;

		dlclose(mp);
		dl_errvalid++;
		strcpy(dl_errbuf, "loadbind: ");
		strerrorcat(dl_errbuf, saverrno);
		return NULL;
	}
	if (readExports(mp) == -1) {
		dlclose(mp);
		return NULL;
	}
	return mp;
}

/*
 * Attempt to decipher an AIX loader error message and append it
 * to our static error message buffer.
 */
static void caterr(char *s)
{
	dTHX;
	dMY_CXT;
	char *p = s;

	while (*p >= '0' && *p <= '9')
		p++;
	switch(atoi(s)) {
	case L_ERROR_TOOMANY:
		strcat(dl_errbuf, "too many errors");
		break;
	case L_ERROR_NOLIB:
		strcat(dl_errbuf, "can't load library");
		strcat(dl_errbuf, p);
		break;
	case L_ERROR_UNDEF:
		strcat(dl_errbuf, "can't find symbol");
		strcat(dl_errbuf, p);
		break;
	case L_ERROR_RLDBAD:
		strcat(dl_errbuf, "bad RLD");
		strcat(dl_errbuf, p);
		break;
	case L_ERROR_FORMAT:
		strcat(dl_errbuf, "bad exec format in");
		strcat(dl_errbuf, p);
		break;
	case L_ERROR_ERRNO:
		strerrorcat(dl_errbuf, atoi(++p));
		break;
	default:
		strcat(dl_errbuf, s);
		break;
	}
}

void *dlsym(void *handle, const char *symbol)
{
	dTHX;
	dMY_CXT;
	ModulePtr mp = (ModulePtr)handle;
	ExportPtr ep;
	int i;

	/*
	 * Could speed up search, but I assume that one assigns
	 * the result to function pointers anyways.
	 */
	for (ep = mp->exports, i = mp->nExports; i; i--, ep++)
		if (strEQ(ep->name, symbol))
			return ep->addr;
	dl_errvalid++;
	strcpy(dl_errbuf, "dlsym: undefined symbol ");
	strcat(dl_errbuf, symbol);
	return NULL;
}

char *dlerror(void)
{
	dTHX;
	dMY_CXT;
	if (dl_errvalid) {
		dl_errvalid = 0;
		return dl_errbuf;
	}
	return NULL;
}

int dlclose(void *handle)
{
	dTHX;
	dMY_CXT;
	ModulePtr mp = (ModulePtr)handle;
	int result;
	ModulePtr mp1;

	if (--mp->refCnt > 0)
		return 0;
	result = UNLOAD(mp->entry);
	if (result == -1) {
		dl_errvalid++;
		strerrorcpy(dl_errbuf, errno);
	}
	if (mp->exports) {
		ExportPtr ep;
		int i;
		for (ep = mp->exports, i = mp->nExports; i; i--, ep++)
			if (ep->name)
				safefree(ep->name);
		safefree(mp->exports);
	}
	if (mp == dl_modList)
		dl_modList = mp->next;
	else {
		for (mp1 = dl_modList; mp1; mp1 = mp1->next)
			if (mp1->next == mp) {
				mp1->next = mp->next;
				break;
			}
	}
	safefree(mp->name);
	safefree(mp);
	return result;
}

/* Added by Wayne Scott 
 * This is needed because the ldopen system call calls
 * calloc to allocated a block of date.  The ldclose call calls free.
 * Without this we get this system calloc and perl's free, resulting
 * in a "Bad free" message.  This way we always use perl's malloc.
 */
void *calloc(size_t ne, size_t sz) 
{
  void *out;

  out = (void *) safemalloc(ne*sz);
  memzero(out, ne*sz);
  return(out);
}
 
/*
 * Build the export table from the XCOFF .loader section.
 */
static int readExports(ModulePtr mp)
{
	dTHX;
	dMY_CXT;
	LDFILE *ldp = NULL;
	AIX_SCNHDR sh;
	AIX_LDHDR *lhp;
	char *ldbuf;
	AIX_LDSYM *ls;
	int i;
	ExportPtr ep;

	if ((ldp = ldopen(mp->name, ldp)) == NULL) {
		struct ld_info *lp;
		char *buf;
		int size = 4*1024;
		if (errno != ENOENT) {
			dl_errvalid++;
			strcpy(dl_errbuf, "readExports: ");
			strerrorcat(dl_errbuf, errno);
			return -1;
		}
		/*
		 * The module might be loaded due to the LIBPATH
		 * environment variable. Search for the loaded
		 * module using L_GETINFO.
		 */
		if ((buf = safemalloc(size)) == NULL) {
			dl_errvalid++;
			strcpy(dl_errbuf, "readExports: ");
			strerrorcat(dl_errbuf, errno);
			return -1;
		}
		while ((i = loadquery(L_GETINFO, buf, size)) == -1 && errno == ENOMEM) {
			safefree(buf);
			size += 4*1024;
			if ((buf = safemalloc(size)) == NULL) {
				dl_errvalid++;
				strcpy(dl_errbuf, "readExports: ");
				strerrorcat(dl_errbuf, errno);
				return -1;
			}
		}
		if (i == -1) {
			dl_errvalid++;
			strcpy(dl_errbuf, "readExports: ");
			strerrorcat(dl_errbuf, errno);
			safefree(buf);
			return -1;
		}
		/*
		 * Traverse the list of loaded modules. The entry point
		 * returned by LOAD() does actually point to the data
		 * segment origin.
		 */
		lp = (struct ld_info *)buf;
		while (lp) {
			if (lp->ldinfo_dataorg == mp->entry) {
				ldp = ldopen(lp->ldinfo_filename, ldp);
				break;
			}
			if (lp->ldinfo_next == 0)
				lp = NULL;
			else
				lp = (struct ld_info *)((char *)lp + lp->ldinfo_next);
		}
		safefree(buf);
		if (!ldp) {
			dl_errvalid++;
			strcpy(dl_errbuf, "readExports: ");
			strerrorcat(dl_errbuf, errno);
			return -1;
		}
	}
#ifdef USE_64_BIT_ALL
	if (TYPE(ldp) != U803XTOCMAGIC) {
#else
	if (TYPE(ldp) != U802TOCMAGIC) {
#endif
		dl_errvalid++;
		strcpy(dl_errbuf, "readExports: bad magic");
		while(ldclose(ldp) == FAILURE)
			;
		return -1;
	}
	if (ldnshread(ldp, _LOADER, &sh) != SUCCESS) {
		dl_errvalid++;
		strcpy(dl_errbuf, "readExports: cannot read loader section header");
		while(ldclose(ldp) == FAILURE)
			;
		return -1;
	}
	/*
	 * We read the complete loader section in one chunk, this makes
	 * finding long symbol names residing in the string table easier.
	 */
	if ((ldbuf = (char *)safemalloc(sh.s_size)) == NULL) {
		dl_errvalid++;
		strcpy(dl_errbuf, "readExports: ");
		strerrorcat(dl_errbuf, errno);
		while(ldclose(ldp) == FAILURE)
			;
		return -1;
	}
	if (FSEEK(ldp, sh.s_scnptr, BEGINNING) != OKFSEEK) {
		dl_errvalid++;
		strcpy(dl_errbuf, "readExports: cannot seek to loader section");
		safefree(ldbuf);
		while(ldclose(ldp) == FAILURE)
			;
		return -1;
	}
/* This first case is a hack, since it assumes that the 3rd parameter to
   FREAD is 1. See the redefinition of FREAD above to see how this works. */
	if (FREAD(ldbuf, sh.s_size, 1, ldp) != 1) {
		dl_errvalid++;
		strcpy(dl_errbuf, "readExports: cannot read loader section");
		safefree(ldbuf);
		while(ldclose(ldp) == FAILURE)
			;
		return -1;
	}
	lhp = (AIX_LDHDR *)ldbuf;
	ls = (AIX_LDSYM *)(ldbuf+AIX_LDHDRSZ);
	/*
	 * Count the number of exports to include in our export table.
	 */
	for (i = lhp->l_nsyms; i; i--, ls++) {
		if (!LDR_EXPORT(*ls))
			continue;
		mp->nExports++;
	}
	Newxz(mp->exports, mp->nExports, Export);
	if (mp->exports == NULL) {
		dl_errvalid++;
		strcpy(dl_errbuf, "readExports: ");
		strerrorcat(dl_errbuf, errno);
		safefree(ldbuf);
		while(ldclose(ldp) == FAILURE)
			;
		return -1;
	}
	/*
	 * Fill in the export table. All entries are relative to
	 * the entry point we got from load.
	 */
	ep = mp->exports;
	ls = (AIX_LDSYM *)(ldbuf+AIX_LDHDRSZ);
	for (i = lhp->l_nsyms; i; i--, ls++) {
		char *symname;
		if (!LDR_EXPORT(*ls))
			continue;
#ifndef USE_64_BIT_ALL
		if (ls->l_zeroes == 0)
#endif
			symname = ls->l_offset+lhp->l_stoff+ldbuf;
#ifndef USE_64_BIT_ALL
		else
			symname = ls->l_name;
#endif
		ep->name = savepv(symname);
		ep->addr = (void *)((unsigned long)mp->entry + ls->l_value);
		ep++;
	}
	safefree(ldbuf);
	while(ldclose(ldp) == FAILURE)
		;
	return 0;
}

/*
 * Find the main modules entry point. This is used as export pointer
 * for loadbind() to be able to resolve references to the main part.
 */
static void * findMain(void)
{
	dTHX;
	dMY_CXT;
	struct ld_info *lp;
	char *buf;
	int size = 4*1024;
	int i;
	void *ret;

	if ((buf = safemalloc(size)) == NULL) {
		dl_errvalid++;
		strcpy(dl_errbuf, "findMain: ");
		strerrorcat(dl_errbuf, errno);
		return NULL;
	}
	while ((i = loadquery(L_GETINFO, buf, size)) == -1 && errno == ENOMEM) {
		safefree(buf);
		size += 4*1024;
		if ((buf = safemalloc(size)) == NULL) {
			dl_errvalid++;
			strcpy(dl_errbuf, "findMain: ");
			strerrorcat(dl_errbuf, errno);
			return NULL;
		}
	}
	if (i == -1) {
		dl_errvalid++;
		strcpy(dl_errbuf, "findMain: ");
		strerrorcat(dl_errbuf, errno);
		safefree(buf);
		return NULL;
	}
	/*
	 * The first entry is the main module. The entry point
	 * returned by load() does actually point to the data
	 * segment origin.
	 */
	lp = (struct ld_info *)buf;
	ret = lp->ldinfo_dataorg;
	safefree(buf);
	return ret;
}
#endif /* USE_NATIVE_DLOPEN */

/* dl_dlopen.xs
 * 
 * Platform:	SunOS/Solaris, possibly others which use dlopen.
 * Author:	Paul Marquess (Paul.Marquess@btinternet.com)
 * Created:	10th July 1994
 *
 * Modified:
 * 15th July 1994   - Added code to explicitly save any error messages.
 * 3rd August 1994  - Upgraded to v3 spec.
 * 9th August 1994  - Changed to use IV
 * 10th August 1994 - Tim Bunce: Added RTLD_LAZY, switchable debugging,
 *                    basic FreeBSD support, removed ClearError
 *
 */

/* Porting notes:

	see dl_dlopen.xs

*/

static void
dl_private_init(pTHX)
{
    (void)dl_generic_private_init(aTHX);
}
 
MODULE = DynaLoader     PACKAGE = DynaLoader

BOOT:
    (void)dl_private_init(aTHX);


void
dl_load_file(filename, flags=0)
	char *	filename
	int	flags
        PREINIT:
        void *retv;
	PPCODE:
	DLDEBUG(1,PerlIO_printf(Perl_debug_log, "dl_load_file(%s,%x):\n", filename,flags));
	if (flags & 0x01)
	    Perl_warn(aTHX_ "Can't make loaded symbols global on this platform while loading %s",filename);
	retv = dlopen(filename, RTLD_GLOBAL|RTLD_LAZY) ;
	DLDEBUG(2,PerlIO_printf(Perl_debug_log, " libref=%x\n", retv));
	ST(0) = sv_newmortal() ;
	if (retv == NULL)
	    SaveError(aTHX_ "%s",dlerror()) ;
	else
	    sv_setiv( ST(0), PTR2IV(retv) );
        XSRETURN(1);

int
dl_unload_file(libref)
    void *	libref
  CODE:
    DLDEBUG(1,PerlIO_printf(Perl_debug_log, "dl_unload_file(%lx):\n", libref));
    RETVAL = (dlclose(libref) == 0 ? 1 : 0);
    if (!RETVAL)
        SaveError(aTHX_ "%s", dlerror()) ;
    DLDEBUG(2,PerlIO_printf(Perl_debug_log, " retval = %d\n", RETVAL));
  OUTPUT:
    RETVAL

void
dl_find_symbol(libhandle, symbolname, ign_err=0)
	void *		libhandle
	char *		symbolname
        int	        ign_err
	PREINIT:
        void *retv;
        CODE:
	DLDEBUG(2,PerlIO_printf(Perl_debug_log, "dl_find_symbol(handle=%x, symbol=%s)\n",
		libhandle, symbolname));
	retv = dlsym(libhandle, symbolname);
	DLDEBUG(2,PerlIO_printf(Perl_debug_log, "  symbolref = %x\n", retv));
	ST(0) = sv_newmortal();
	if (retv == NULL) {
            if (!ign_err)
	        SaveError(aTHX_ "%s", dlerror());
	} else
	    sv_setiv( ST(0), PTR2IV(retv));


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


SV *
dl_error()
    CODE:
    dMY_CXT;
    RETVAL = newSVsv(MY_CXT.x_dl_last_error);
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
    MY_CXT.x_dl_last_error = newSVpvs("");

#endif

# end.
