/* VMS::Stdio - VMS extensions to stdio routines 
 *
 * Version:  2.0
 * Author:   Charles Bailey  bailey@genetics.upenn.edu
 * Revised:  28-Feb-1996
 *
 */

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include <file.h>

static bool
constant(name, pval)
char *name;
IV *pval;
{
    if (strnNE(name, "O_", 2)) return FALSE;

    if (strEQ(name, "O_APPEND"))
#ifdef O_APPEND
	{ *pval = O_APPEND; return TRUE; }
#else
	return FALSE;
#endif
    if (strEQ(name, "O_CREAT"))
#ifdef O_CREAT
	{ *pval = O_CREAT; return TRUE; }
#else
	return FALSE;
#endif
    if (strEQ(name, "O_EXCL"))
#ifdef O_EXCL
	{ *pval = O_EXCL; return TRUE; }
#else
	return FALSE;
#endif
    if (strEQ(name, "O_NDELAY"))
#ifdef O_NDELAY
	{ *pval = O_NDELAY; return TRUE; }
#else
	return FALSE;
#endif
    if (strEQ(name, "O_NOWAIT"))
#ifdef O_NOWAIT
	{ *pval = O_NOWAIT; return TRUE; }
#else
	return FALSE;
#endif
    if (strEQ(name, "O_RDONLY"))
#ifdef O_RDONLY
	{ *pval = O_RDONLY; return TRUE; }
#else
	return FALSE;
#endif
    if (strEQ(name, "O_RDWR"))
#ifdef O_RDWR
	{ *pval = O_RDWR; return TRUE; }
#else
	return FALSE;
#endif
    if (strEQ(name, "O_TRUNC"))
#ifdef O_TRUNC
	{ *pval = O_TRUNC; return TRUE; }
#else
	return FALSE;
#endif
    if (strEQ(name, "O_WRONLY"))
#ifdef O_WRONLY
	{ *pval = O_WRONLY; return TRUE; }
#else
	return FALSE;
#endif

    return FALSE;
}


static SV *
newFH(FILE *fp, char type) {
    SV *rv, *gv = NEWSV(0,0);
    GV **stashp;
    HV *stash;
    IO *io;

    /* Find stash for VMS::Stdio.  We don't do this once at boot
     * to allow for possibility of threaded Perl with per-thread
     * symbol tables.  This code (through io = ...) is really
     * equivalent to gv_fetchpv("VMS::Stdio::__FH__",TRUE,SVt_PVIO),
     * with a little less overhead, and good exercise for me. :-) */
    stashp = (GV **)hv_fetch(defstash,"VMS::",5,TRUE);
    if (!stashp || *stashp == (GV *)&sv_undef) return Nullsv;
    if (!(stash = GvHV(*stashp))) stash = GvHV(*stashp) = newHV();
    stashp = (GV **)hv_fetch(GvHV(*stashp),"Stdio::",7,TRUE);
    if (!stashp || *stashp == (GV *)&sv_undef) return Nullsv;
    if (!(stash = GvHV(*stashp))) stash = GvHV(*stashp) = newHV();

    /* Set up GV to point to IO, and then take reference */
    gv_init(gv,stash,"__FH__",6,0);
    io = GvIOp(gv) = newIO();
    IoIFP(io) = fp;
    if (type != '>') IoOFP(io) = fp;
    IoTYPE(io) = type;
    rv = newRV(gv);
    SvREFCNT_dec(gv);
    return sv_bless(rv,stash);
}

MODULE = VMS::Stdio  PACKAGE = VMS::Stdio

void
constant(name)
	char *	name
	PROTOTYPE: $
	CODE:
	IV i;
	if (constant(name, &i))
	    ST(0) = sv_2mortal(newSViv(i));
	else
	    ST(0) = &sv_undef;

void
flush(sv)
	SV *	sv
	PROTOTYPE: $
	CODE:
	    FILE *fp = Nullfp;
	    if (SvOK(sv)) fp = IoIFP(sv_2io(sv));
	    ST(0) = fflush(fp) ? &sv_undef : &sv_yes;

char *
getname(fp)
	FILE *	fp
	PROTOTYPE: $
	CODE:
	    char fname[257];
	    ST(0) = sv_newmortal();
	    if (fgetname(fp,fname) != NULL) sv_setpv(ST(0),fname);

void
rewind(fp)
	FILE *	fp
	PROTOTYPE: $
	CODE:
	    ST(0) = rewind(fp) ? &sv_undef : &sv_yes;

void
remove(name)
	char *name
	PROTOTYPE: $
	CODE:
	    ST(0) = remove(name) ? &sv_undef : &sv_yes;

void
sync(fp)
	FILE *	fp
	PROTOTYPE: $
	CODE:
	    ST(0) = fsync(fileno(fp)) ? &sv_undef : &sv_yes;

char *
tmpnam()
	PROTOTYPE:
	CODE:
	    char fname[L_tmpnam];
	    ST(0) = sv_newmortal();
	    if (tmpnam(fname) != NULL) sv_setpv(ST(0),fname);

void
vmsopen(spec,...)
	char *	spec
	PROTOTYPE: @
	CODE:
	    char *args[8],mode[3] = {'r','\0','\0'}, type = '<';
	    register int i, myargc;
	    FILE *fp;
	
	    if (!spec || !*spec) {
	       SETERRNO(EINVAL,LIB$_INVARG);
	       XSRETURN_UNDEF;
	    }
	    if (items > 9) croak("too many args");
	
	    /* First, set up name and mode args from perl's string */
	    if (*spec == '+') {
	      mode[1] = '+';
	      spec++;
	    }
	    if (*spec == '>') {
	      if (*(spec+1) == '>') *mode = 'a', spec += 2;
	      else *mode = 'w',  spec++;
	    }
	    else if (*spec == '<') spec++;
	    myargc = items - 1;
	    for (i = 0; i < myargc; i++) args[i] = SvPV(ST(i+1),na);
	    /* This hack brought to you by C's opaque arglist management */
	    switch (myargc) {
	      case 0:
	        fp = fopen(spec,mode);
	        break;
	      case 1:
	        fp = fopen(spec,mode,args[0]);
	        break;
	      case 2:
	        fp = fopen(spec,mode,args[0],args[1]);
	        break;
	      case 3:
	        fp = fopen(spec,mode,args[0],args[1],args[2]);
	        break;
	      case 4:
	        fp = fopen(spec,mode,args[0],args[1],args[2],args[3]);
	        break;
	      case 5:
	        fp = fopen(spec,mode,args[0],args[1],args[2],args[3],args[4]);
	        break;
	      case 6:
	        fp = fopen(spec,mode,args[0],args[1],args[2],args[3],args[4],args[5]);
	        break;
	      case 7:
	        fp = fopen(spec,mode,args[0],args[1],args[2],args[3],args[4],args[5],args[6]);
	        break;
	      case 8:
	        fp = fopen(spec,mode,args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7]);
	        break;
	    }
	    if (fp != Nullfp) {
	      SV *fh = newFH(fp,(mode[1] ? '+' : (mode[0] == 'r' ? '<' : '>')));
	      ST(0) = (fh ? sv_2mortal(fh) : &sv_undef);
	    }
	    else { ST(0) = &sv_undef; }

void
vmssysopen(spec,mode,perm,...)
	char *	spec
	int	mode
	int	perm
	PROTOTYPE: @
	CODE:
	    char *args[8];
	    int i, myargc, fd;
	    FILE *fp;
	    SV *fh;
	    if (!spec || !*spec) {
	       SETERRNO(EINVAL,LIB$_INVARG);
	       XSRETURN_UNDEF;
	    }
	    if (items > 11) croak("too many args");
	    myargc = items - 3;
	    for (i = 0; i < myargc; i++) args[i] = SvPV(ST(i+3),na);
	    /* More fun with C calls; can't combine with above because
	       args 2,3 of different types in fopen() and open() */
	    switch (myargc) {
	      case 0:
	        fd = open(spec,mode,perm);
	        break;
	      case 1:
	        fd = open(spec,mode,perm,args[0]);
	        break;
	      case 2:
	        fd = open(spec,mode,perm,args[0],args[1]);
	        break;
	      case 3:
	        fd = open(spec,mode,perm,args[0],args[1],args[2]);
	        break;
	      case 4:
	        fd = open(spec,mode,perm,args[0],args[1],args[2],args[3]);
	        break;
	      case 5:
	        fd = open(spec,mode,perm,args[0],args[1],args[2],args[3],args[4]);
	        break;
	      case 6:
	        fd = open(spec,mode,perm,args[0],args[1],args[2],args[3],args[4],args[5]);
	        break;
	      case 7:
	        fd = open(spec,mode,perm,args[0],args[1],args[2],args[3],args[4],args[5],args[6]);
	        break;
	      case 8:
	        fd = open(spec,mode,perm,args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7]);
	        break;
	    }
	    i = mode & 3;
	    if (fd >= 0 &&
	       ((fp = fdopen(fd, &("r\000w\000r+"[2*i]))) != Nullfp)) {
	      SV *fh = newFH(fp,"<>++"[i]);
	      ST(0) = (fh ? sv_2mortal(fh) : &sv_undef);
	    }
	    else { ST(0) = &sv_undef; }

void
waitfh(fp)
	FILE *	fp
	PROTOTYPE: $
	CODE:
	    ST(0) = fwait(fp) ? &sv_undef : &sv_yes;
