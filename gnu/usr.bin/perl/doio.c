/*    doio.c
 *
 *    Copyright (c) 1991-1994, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "Far below them they saw the white waters pour into a foaming bowl, and
 * then swirl darkly about a deep oval basin in the rocks, until they found
 * their way out again through a narrow gate, and flowed away, fuming and
 * chattering, into calmer and more level reaches."
 */

#include "EXTERN.h"
#include "perl.h"

#if defined(HAS_MSG) || defined(HAS_SEM) || defined(HAS_SHM)
#include <sys/ipc.h>
#ifdef HAS_MSG
#include <sys/msg.h>
#endif
#ifdef HAS_SEM
#include <sys/sem.h>
#endif
#ifdef HAS_SHM
#include <sys/shm.h>
# ifndef HAS_SHMAT_PROTOTYPE
    extern Shmat_t shmat _((int, char *, int));
# endif
#endif
#endif

#ifdef I_UTIME
#include <utime.h>
#endif
#ifdef I_FCNTL
#include <fcntl.h>
#endif
#ifdef I_SYS_FILE
#include <sys/file.h>
#endif

#if defined(HAS_SOCKET) && !defined(VMS) /* VMS handles sockets via vmsish.h */
# include <sys/socket.h>
# include <netdb.h>
# ifndef ENOTSOCK
#  ifdef I_NET_ERRNO
#   include <net/errno.h>
#  endif
# endif
#endif

bool
do_open(gv,name,len,as_raw,rawmode,rawperm,supplied_fp)
GV *gv;
register char *name;
I32 len;
int as_raw;
int rawmode, rawperm;
FILE *supplied_fp;
{
    register IO *io = GvIOn(gv);
    FILE *saveifp = Nullfp;
    FILE *saveofp = Nullfp;
    char savetype = ' ';
    int writing = 0;
    FILE *fp;
    int fd;
    int result;

    forkprocess = 1;		/* assume true if no fork */

    if (IoIFP(io)) {
	fd = fileno(IoIFP(io));
	if (IoTYPE(io) == '-')
	    result = 0;
	else if (fd <= maxsysfd) {
	    saveifp = IoIFP(io);
	    saveofp = IoOFP(io);
	    savetype = IoTYPE(io);
	    result = 0;
	}
	else if (IoTYPE(io) == '|')
	    result = my_pclose(IoIFP(io));
	else if (IoIFP(io) != IoOFP(io)) {
	    if (IoOFP(io)) {
		result = fclose(IoOFP(io));
		fclose(IoIFP(io));	/* clear stdio, fd already closed */
	    }
	    else
		result = fclose(IoIFP(io));
	}
	else
	    result = fclose(IoIFP(io));
	if (result == EOF && fd > maxsysfd)
	    fprintf(stderr,"Warning: unable to close filehandle %s properly.\n",
	      GvENAME(gv));
	IoOFP(io) = IoIFP(io) = Nullfp;
    }

    if (as_raw) {
	result = rawmode & 3;
	IoTYPE(io) = "<>++"[result];
	writing = (result > 0);
	fd = open(name, rawmode, rawperm);
	if (fd == -1)
	    fp = NULL;
	else {
	    fp = fdopen(fd, ((result == 0) ? "r"
			     : (result == 1) ? "w"
			     : "r+"));
	    if (!fp)
		close(fd);
	}
    }
    else {
	char *myname;
	char mode[3];		/* stdio file mode ("r\0" or "r+\0") */
	int dodup;

	myname = savepvn(name, len);
	SAVEFREEPV(myname);
	name = myname;
	while (len && isSPACE(name[len-1]))
	    name[--len] = '\0';

	mode[0] = mode[1] = mode[2] = '\0';
	IoTYPE(io) = *name;
	if (*name == '+' && len > 1 && name[len-1] != '|') { /* scary */
	    mode[1] = *name++;
	    --len;
	    writing = 1;
	}

	if (*name == '|') {
	    /*SUPPRESS 530*/
	    for (name++; isSPACE(*name); name++) ;
	    if (strNE(name,"-"))
		TAINT_ENV();
	    TAINT_PROPER("piped open");
	    if (dowarn && name[strlen(name)-1] == '|')
		warn("Can't do bidirectional pipe");
	    fp = my_popen(name,"w");
	    writing = 1;
	}
	else if (*name == '>') {
	    TAINT_PROPER("open");
	    name++;
	    if (*name == '>') {
		mode[0] = IoTYPE(io) = 'a';
		name++;
	    }
	    else
		mode[0] = 'w';
	    writing = 1;

	    if (*name == '&') {
	      duplicity:
		dodup = 1;
		name++;
		if (*name == '=') {
		    dodup = 0;
		    name++;
		}
		if (!*name && supplied_fp)
		    fp = supplied_fp;
		else {
		    /*SUPPRESS 530*/
		    for (; isSPACE(*name); name++) ;
		    if (isDIGIT(*name))
			fd = atoi(name);
		    else {
			IO* thatio;
			gv = gv_fetchpv(name,FALSE,SVt_PVIO);
			thatio = GvIO(gv);
			if (!thatio) {
#ifdef EINVAL
			    SETERRNO(EINVAL,SS$_IVCHAN);
#endif
			    goto say_false;
			}
			if (IoIFP(thatio)) {
			    fd = fileno(IoIFP(thatio));
			    if (IoTYPE(thatio) == 's')
				IoTYPE(io) = 's';
			}
			else
			    fd = -1;
		    }
		    if (dodup)
			fd = dup(fd);
		    if (!(fp = fdopen(fd,mode)))
			if (dodup)
			    close(fd);
		}
	    }
	    else {
		/*SUPPRESS 530*/
		for (; isSPACE(*name); name++) ;
		if (strEQ(name,"-")) {
		    fp = stdout;
		    IoTYPE(io) = '-';
		}
		else  {
		    fp = fopen(name,mode);
		}
	    }
	}
	else if (*name == '<') {
	    /*SUPPRESS 530*/
	    for (name++; isSPACE(*name); name++) ;
	    mode[0] = 'r';
	    if (*name == '&')
		goto duplicity;
	    if (strEQ(name,"-")) {
		fp = stdin;
		IoTYPE(io) = '-';
	    }
	    else
		fp = fopen(name,mode);
	}
	else if (name[len-1] == '|') {
	    name[--len] = '\0';
	    while (len && isSPACE(name[len-1]))
		name[--len] = '\0';
	    /*SUPPRESS 530*/
	    for (; isSPACE(*name); name++) ;
	    if (strNE(name,"-"))
		TAINT_ENV();
	    TAINT_PROPER("piped open");
	    fp = my_popen(name,"r");
	    IoTYPE(io) = '|';
	}
	else {
	    IoTYPE(io) = '<';
	    /*SUPPRESS 530*/
	    for (; isSPACE(*name); name++) ;
	    if (strEQ(name,"-")) {
		fp = stdin;
		IoTYPE(io) = '-';
	    }
	    else
		fp = fopen(name,"r");
	}
    }
    if (!fp) {
	if (dowarn && IoTYPE(io) == '<' && strchr(name, '\n'))
	    warn(warn_nl, "open");
	goto say_false;
    }
    if (IoTYPE(io) &&
      IoTYPE(io) != '|' && IoTYPE(io) != '-') {
	if (Fstat(fileno(fp),&statbuf) < 0) {
	    (void)fclose(fp);
	    goto say_false;
	}
	if (S_ISSOCK(statbuf.st_mode))
	    IoTYPE(io) = 's';	/* in case a socket was passed in to us */
#ifdef HAS_SOCKET
	else if (
#ifdef S_IFMT
	    !(statbuf.st_mode & S_IFMT)
#else
	    !statbuf.st_mode
#endif
	) {
	    int buflen = sizeof tokenbuf;
	    if (getsockname(fileno(fp), (struct sockaddr *)tokenbuf, &buflen) >= 0
		|| errno != ENOTSOCK)
		IoTYPE(io) = 's'; /* some OS's return 0 on fstat()ed socket */
				/* but some return 0 for streams too, sigh */
	}
#endif
    }
    if (saveifp) {		/* must use old fp? */
	fd = fileno(saveifp);
	if (saveofp) {
	    Fflush(saveofp);		/* emulate fclose() */
	    if (saveofp != saveifp) {	/* was a socket? */
		fclose(saveofp);
		if (fd > 2)
		    Safefree(saveofp);
	    }
	}
	if (fd != fileno(fp)) {
	    int pid;
	    SV *sv;

	    dup2(fileno(fp), fd);
	    sv = *av_fetch(fdpid,fileno(fp),TRUE);
	    (void)SvUPGRADE(sv, SVt_IV);
	    pid = SvIVX(sv);
	    SvIVX(sv) = 0;
	    sv = *av_fetch(fdpid,fd,TRUE);
	    (void)SvUPGRADE(sv, SVt_IV);
	    SvIVX(sv) = pid;
	    fclose(fp);

	}
	fp = saveifp;
	clearerr(fp);
    }
#if defined(HAS_FCNTL) && defined(F_SETFD)
    fd = fileno(fp);
    fcntl(fd,F_SETFD,fd > maxsysfd);
#endif
    IoIFP(io) = fp;
    if (writing) {
	if (IoTYPE(io) == 's'
	  || (IoTYPE(io) == '>' && S_ISCHR(statbuf.st_mode)) ) {
	    if (!(IoOFP(io) = fdopen(fileno(fp),"w"))) {
		fclose(fp);
		IoIFP(io) = Nullfp;
		goto say_false;
	    }
	}
	else
	    IoOFP(io) = fp;
    }
    return TRUE;

say_false:
    IoIFP(io) = saveifp;
    IoOFP(io) = saveofp;
    IoTYPE(io) = savetype;
    return FALSE;
}

FILE *
nextargv(gv)
register GV *gv;
{
    register SV *sv;
#ifndef FLEXFILENAMES
    int filedev;
    int fileino;
#endif
    int fileuid;
    int filegid;

    if (!argvoutgv)
	argvoutgv = gv_fetchpv("ARGVOUT",TRUE,SVt_PVIO);
    if (filemode & (S_ISUID|S_ISGID)) {
	Fflush(IoIFP(GvIOn(argvoutgv)));  /* chmod must follow last write */
#ifdef HAS_FCHMOD
	(void)fchmod(lastfd,filemode);
#else
	(void)chmod(oldname,filemode);
#endif
    }
    filemode = 0;
    while (av_len(GvAV(gv)) >= 0) {
	STRLEN len;
	sv = av_shift(GvAV(gv));
	SAVEFREESV(sv);
	sv_setsv(GvSV(gv),sv);
	SvSETMAGIC(GvSV(gv));
	oldname = SvPVx(GvSV(gv), len);
	if (do_open(gv,oldname,len,FALSE,0,0,Nullfp)) {
	    if (inplace) {
		TAINT_PROPER("inplace open");
		if (strEQ(oldname,"-")) {
		    setdefout(gv_fetchpv("STDOUT",TRUE,SVt_PVIO));
		    return IoIFP(GvIOp(gv));
		}
#ifndef FLEXFILENAMES
		filedev = statbuf.st_dev;
		fileino = statbuf.st_ino;
#endif
		filemode = statbuf.st_mode;
		fileuid = statbuf.st_uid;
		filegid = statbuf.st_gid;
		if (!S_ISREG(filemode)) {
		    warn("Can't do inplace edit: %s is not a regular file",
		      oldname );
		    do_close(gv,FALSE);
		    continue;
		}
		if (*inplace) {
#ifdef SUFFIX
		    add_suffix(sv,inplace);
#else
		    sv_catpv(sv,inplace);
#endif
#ifndef FLEXFILENAMES
		    if (Stat(SvPVX(sv),&statbuf) >= 0
		      && statbuf.st_dev == filedev
		      && statbuf.st_ino == fileino ) {
			warn("Can't do inplace edit: %s > 14 characters",
			  SvPVX(sv) );
			do_close(gv,FALSE);
			continue;
		    }
#endif
#ifdef HAS_RENAME
#ifndef DOSISH
		    if (rename(oldname,SvPVX(sv)) < 0) {
			warn("Can't rename %s to %s: %s, skipping file",
			  oldname, SvPVX(sv), Strerror(errno) );
			do_close(gv,FALSE);
			continue;
		    }
#else
		    do_close(gv,FALSE);
		    (void)unlink(SvPVX(sv));
		    (void)rename(oldname,SvPVX(sv));
		    do_open(gv,SvPVX(sv),SvCUR(sv),FALSE,0,0,Nullfp);
#endif /* MSDOS */
#else
		    (void)UNLINK(SvPVX(sv));
		    if (link(oldname,SvPVX(sv)) < 0) {
			warn("Can't rename %s to %s: %s, skipping file",
			  oldname, SvPVX(sv), Strerror(errno) );
			do_close(gv,FALSE);
			continue;
		    }
		    (void)UNLINK(oldname);
#endif
		}
		else {
#ifndef DOSISH
		    if (UNLINK(oldname) < 0) {
			warn("Can't rename %s to %s: %s, skipping file",
			  oldname, SvPVX(sv), Strerror(errno) );
			do_close(gv,FALSE);
			continue;
		    }
#else
		    croak("Can't do inplace edit without backup");
#endif
		}

		sv_setpvn(sv,">",1);
		sv_catpv(sv,oldname);
		SETERRNO(0,0);		/* in case sprintf set errno */
		if (!do_open(argvoutgv,SvPVX(sv),SvCUR(sv),FALSE,0,0,Nullfp)) {
		    warn("Can't do inplace edit on %s: %s",
		      oldname, Strerror(errno) );
		    do_close(gv,FALSE);
		    continue;
		}
		setdefout(argvoutgv);
		lastfd = fileno(IoIFP(GvIOp(argvoutgv)));
		(void)Fstat(lastfd,&statbuf);
#ifdef HAS_FCHMOD
		(void)fchmod(lastfd,filemode);
#else
		(void)chmod(oldname,filemode);
#endif
		if (fileuid != statbuf.st_uid || filegid != statbuf.st_gid) {
#ifdef HAS_FCHOWN
		    (void)fchown(lastfd,fileuid,filegid);
#else
#ifdef HAS_CHOWN
		    (void)chown(oldname,fileuid,filegid);
#endif
#endif
		}
	    }
	    return IoIFP(GvIOp(gv));
	}
	else
	    fprintf(stderr,"Can't open %s: %s\n",SvPV(sv, na), Strerror(errno));
    }
    if (inplace) {
	(void)do_close(argvoutgv,FALSE);
	setdefout(gv_fetchpv("STDOUT",TRUE,SVt_PVIO));
    }
    return Nullfp;
}

#ifdef HAS_PIPE
void
do_pipe(sv, rgv, wgv)
SV *sv;
GV *rgv;
GV *wgv;
{
    register IO *rstio;
    register IO *wstio;
    int fd[2];

    if (!rgv)
	goto badexit;
    if (!wgv)
	goto badexit;

    rstio = GvIOn(rgv);
    wstio = GvIOn(wgv);

    if (IoIFP(rstio))
	do_close(rgv,FALSE);
    if (IoIFP(wstio))
	do_close(wgv,FALSE);

    if (pipe(fd) < 0)
	goto badexit;
    IoIFP(rstio) = fdopen(fd[0], "r");
    IoOFP(wstio) = fdopen(fd[1], "w");
    IoIFP(wstio) = IoOFP(wstio);
    IoTYPE(rstio) = '<';
    IoTYPE(wstio) = '>';
    if (!IoIFP(rstio) || !IoOFP(wstio)) {
	if (IoIFP(rstio)) fclose(IoIFP(rstio));
	else close(fd[0]);
	if (IoOFP(wstio)) fclose(IoOFP(wstio));
	else close(fd[1]);
	goto badexit;
    }

    sv_setsv(sv,&sv_yes);
    return;

badexit:
    sv_setsv(sv,&sv_undef);
    return;
}
#endif

bool
#ifndef CAN_PROTOTYPE
do_close(gv,explicit)
GV *gv;
bool explicit;
#else
do_close(GV *gv, bool explicit)
#endif /* CAN_PROTOTYPE */
{
    bool retval;
    IO *io;

    if (!gv)
	gv = argvgv;
    if (!gv || SvTYPE(gv) != SVt_PVGV) {
	SETERRNO(EBADF,SS$_IVCHAN);
	return FALSE;
    }
    io = GvIO(gv);
    if (!io) {		/* never opened */
	if (dowarn && explicit)
	    warn("Close on unopened file <%s>",GvENAME(gv));
	return FALSE;
    }
    retval = io_close(io);
    if (explicit) {
	IoLINES(io) = 0;
	IoPAGE(io) = 0;
	IoLINES_LEFT(io) = IoPAGE_LEN(io);
    }
    IoTYPE(io) = ' ';
    return retval;
}

bool
io_close(io)
IO* io;
{
    bool retval = FALSE;
    int status;

    if (IoIFP(io)) {
	if (IoTYPE(io) == '|') {
	    status = my_pclose(IoIFP(io));
	    retval = (status == 0);
	    statusvalue = FIXSTATUS(status);
	}
	else if (IoTYPE(io) == '-')
	    retval = TRUE;
	else {
	    if (IoOFP(io) && IoOFP(io) != IoIFP(io)) {		/* a socket */
		retval = (fclose(IoOFP(io)) != EOF);
		fclose(IoIFP(io));	/* clear stdio, fd already closed */
	    }
	    else
		retval = (fclose(IoIFP(io)) != EOF);
	}
	IoOFP(io) = IoIFP(io) = Nullfp;
    }

    return retval;
}

bool
do_eof(gv)
GV *gv;
{
    register IO *io;
    int ch;

    io = GvIO(gv);

    if (!io)
	return TRUE;

    while (IoIFP(io)) {

#ifdef USE_STDIO_PTR			/* (the code works without this) */
	if (FILE_cnt(IoIFP(io)) > 0)	/* cheat a little, since */
	    return FALSE;		/* this is the most usual case */
#endif

	ch = getc(IoIFP(io));
	if (ch != EOF) {
	    (void)ungetc(ch, IoIFP(io));
	    return FALSE;
	}
#if defined(USE_STDIO_PTR) && defined(STDIO_CNT_LVALUE)
	if (FILE_cnt(IoIFP(io)) < -1)
	    FILE_cnt(IoIFP(io)) = -1;
#endif
	if (op->op_flags & OPf_SPECIAL) { /* not necessarily a real EOF yet? */
	    if (!nextargv(argvgv))	/* get another fp handy */
		return TRUE;
	}
	else
	    return TRUE;		/* normal fp, definitely end of file */
    }
    return TRUE;
}

long
do_tell(gv)
GV *gv;
{
    register IO *io;

    if (!gv)
	goto phooey;

    io = GvIO(gv);
    if (!io || !IoIFP(io))
	goto phooey;

#ifdef ULTRIX_STDIO_BOTCH
    if (feof(IoIFP(io)))
	(void)fseek (IoIFP(io), 0L, 2);		/* ultrix 1.2 workaround */
#endif

    return ftell(IoIFP(io));

phooey:
    if (dowarn)
	warn("tell() on unopened file");
    SETERRNO(EBADF,RMS$_IFI);
    return -1L;
}

bool
do_seek(gv, pos, whence)
GV *gv;
long pos;
int whence;
{
    register IO *io;

    if (!gv)
	goto nuts;

    io = GvIO(gv);
    if (!io || !IoIFP(io))
	goto nuts;

#ifdef ULTRIX_STDIO_BOTCH
    if (feof(IoIFP(io)))
	(void)fseek (IoIFP(io), 0L, 2);		/* ultrix 1.2 workaround */
#endif

    return fseek(IoIFP(io), pos, whence) >= 0;

nuts:
    if (dowarn)
	warn("seek() on unopened file");
    SETERRNO(EBADF,RMS$_IFI);
    return FALSE;
}

#if !defined(HAS_TRUNCATE) && !defined(HAS_CHSIZE) && defined(F_FREESP)
	/* code courtesy of William Kucharski */
#define HAS_CHSIZE

I32 chsize(fd, length)
I32 fd;			/* file descriptor */
Off_t length;		/* length to set file to */
{
    extern long lseek();
    struct flock fl;
    struct stat filebuf;

    if (Fstat(fd, &filebuf) < 0)
	return -1;

    if (filebuf.st_size < length) {

	/* extend file length */

	if ((lseek(fd, (length - 1), 0)) < 0)
	    return -1;

	/* write a "0" byte */

	if ((write(fd, "", 1)) != 1)
	    return -1;
    }
    else {
	/* truncate length */

	fl.l_whence = 0;
	fl.l_len = 0;
	fl.l_start = length;
	fl.l_type = F_WRLCK;    /* write lock on file space */

	/*
	* This relies on the UNDOCUMENTED F_FREESP argument to
	* fcntl(2), which truncates the file so that it ends at the
	* position indicated by fl.l_start.
	*
	* Will minor miracles never cease?
	*/

	if (fcntl(fd, F_FREESP, &fl) < 0)
	    return -1;

    }

    return 0;
}
#endif /* F_FREESP */

I32
looks_like_number(sv)
SV *sv;
{
    register char *s;
    register char *send;

    if (!SvPOK(sv)) {
	STRLEN len;
	if (!SvPOKp(sv))
	    return TRUE;
	s = SvPV(sv, len);
	send = s + len;
    }
    else {
	s = SvPVX(sv); 
	send = s + SvCUR(sv);
    }
    while (isSPACE(*s))
	s++;
    if (s >= send)
	return FALSE;
    if (*s == '+' || *s == '-')
	s++;
    while (isDIGIT(*s))
	s++;
    if (s == send)
	return TRUE;
    if (*s == '.') 
	s++;
    else if (s == SvPVX(sv))
	return FALSE;
    while (isDIGIT(*s))
	s++;
    if (s == send)
	return TRUE;
    if (*s == 'e' || *s == 'E') {
	s++;
	if (*s == '+' || *s == '-')
	    s++;
	while (isDIGIT(*s))
	    s++;
    }
    while (isSPACE(*s))
	s++;
    if (s >= send)
	return TRUE;
    return FALSE;
}

bool
do_print(sv,fp)
register SV *sv;
FILE *fp;
{
    register char *tmps;
    STRLEN len;

    /* assuming fp is checked earlier */
    if (!sv)
	return TRUE;
    if (ofmt) {
	if (SvGMAGICAL(sv))
	    mg_get(sv);
        if (SvIOK(sv) && SvIVX(sv) != 0) {
	    fprintf(fp, ofmt, (double)SvIVX(sv));
	    return !ferror(fp);
	}
	if (  (SvNOK(sv) && SvNVX(sv) != 0.0)
	   || (looks_like_number(sv) && sv_2nv(sv) != 0.0) ) {
	    fprintf(fp, ofmt, SvNVX(sv));
	    return !ferror(fp);
	}
    }
    switch (SvTYPE(sv)) {
    case SVt_NULL:
	if (dowarn)
	    warn(warn_uninit);
	return TRUE;
    case SVt_IV:
	if (SvIOK(sv)) {
	    if (SvGMAGICAL(sv))
		mg_get(sv);
	    fprintf(fp, "%ld", (long)SvIVX(sv));
	    return !ferror(fp);
	}
	/* FALL THROUGH */
    default:
	tmps = SvPV(sv, len);
	break;
    }
    if (len && (fwrite1(tmps,1,len,fp) == 0 || ferror(fp)))
	return FALSE;
    return TRUE;
}

I32
my_stat(ARGS)
dARGS
{
    dSP;
    IO *io;
    GV* tmpgv;

    if (op->op_flags & OPf_REF) {
	EXTEND(sp,1);
	tmpgv = cGVOP->op_gv;
      do_fstat:
	io = GvIO(tmpgv);
	if (io && IoIFP(io)) {
	    statgv = tmpgv;
	    sv_setpv(statname,"");
	    laststype = OP_STAT;
	    return (laststatval = Fstat(fileno(IoIFP(io)), &statcache));
	}
	else {
	    if (tmpgv == defgv)
		return laststatval;
	    if (dowarn)
		warn("Stat on unopened file <%s>",
		  GvENAME(tmpgv));
	    statgv = Nullgv;
	    sv_setpv(statname,"");
	    return (laststatval = -1);
	}
    }
    else {
	SV* sv = POPs;
	PUTBACK;
	if (SvTYPE(sv) == SVt_PVGV) {
	    tmpgv = (GV*)sv;
	    goto do_fstat;
	}
	else if (SvROK(sv) && SvTYPE(SvRV(sv)) == SVt_PVGV) {
	    tmpgv = (GV*)SvRV(sv);
	    goto do_fstat;
	}

	statgv = Nullgv;
	sv_setpv(statname,SvPV(sv, na));
	laststype = OP_STAT;
	laststatval = Stat(SvPV(sv, na),&statcache);
	if (laststatval < 0 && dowarn && strchr(SvPV(sv, na), '\n'))
	    warn(warn_nl, "stat");
	return laststatval;
    }
}

I32
my_lstat(ARGS)
dARGS
{
    dSP;
    SV *sv;
    if (op->op_flags & OPf_REF) {
	EXTEND(sp,1);
	if (cGVOP->op_gv == defgv) {
	    if (laststype != OP_LSTAT)
		croak("The stat preceding -l _ wasn't an lstat");
	    return laststatval;
	}
	croak("You can't use -l on a filehandle");
    }

    laststype = OP_LSTAT;
    statgv = Nullgv;
    sv = POPs;
    PUTBACK;
    sv_setpv(statname,SvPV(sv, na));
#ifdef HAS_LSTAT
    laststatval = lstat(SvPV(sv, na),&statcache);
#else
    laststatval = Stat(SvPV(sv, na),&statcache);
#endif
    if (laststatval < 0 && dowarn && strchr(SvPV(sv, na), '\n'))
	warn(warn_nl, "lstat");
    return laststatval;
}

bool
do_aexec(really,mark,sp)
SV *really;
register SV **mark;
register SV **sp;
{
    register char **a;
    char *tmps;

    if (sp > mark) {
	New(401,Argv, sp - mark + 1, char*);
	a = Argv;
	while (++mark <= sp) {
	    if (*mark)
		*a++ = SvPVx(*mark, na);
	    else
		*a++ = "";
	}
	*a = Nullch;
	if (*Argv[0] != '/')	/* will execvp use PATH? */
	    TAINT_ENV();		/* testing IFS here is overkill, probably */
	if (really && *(tmps = SvPV(really, na)))
	    execvp(tmps,Argv);
	else
	    execvp(Argv[0],Argv);
	if (dowarn)
	    warn("Can't exec \"%s\": %s", Argv[0], Strerror(errno));
    }
    do_execfree();
    return FALSE;
}

void
do_execfree()
{
    if (Argv) {
	Safefree(Argv);
	Argv = Null(char **);
    }
    if (Cmd) {
	Safefree(Cmd);
	Cmd = Nullch;
    }
}

bool
do_exec(cmd)
char *cmd;
{
    register char **a;
    register char *s;
    char flags[10];

    while (*cmd && isSPACE(*cmd))
	cmd++;

    /* save an extra exec if possible */

#ifdef CSH
    if (strnEQ(cmd,cshname,cshlen) && strnEQ(cmd+cshlen," -c",3)) {
	strcpy(flags,"-c");
	s = cmd+cshlen+3;
	if (*s == 'f') {
	    s++;
	    strcat(flags,"f");
	}
	if (*s == ' ')
	    s++;
	if (*s++ == '\'') {
	    char *ncmd = s;

	    while (*s)
		s++;
	    if (s[-1] == '\n')
		*--s = '\0';
	    if (s[-1] == '\'') {
		*--s = '\0';
		execl(cshname,"csh", flags,ncmd,(char*)0);
		*s = '\'';
		return FALSE;
	    }
	}
    }
#endif /* CSH */

    /* see if there are shell metacharacters in it */

    if (*cmd == '.' && isSPACE(cmd[1]))
	goto doshell;

    if (strnEQ(cmd,"exec",4) && isSPACE(cmd[4]))
	goto doshell;

    for (s = cmd; *s && isALPHA(*s); s++) ;	/* catch VAR=val gizmo */
    if (*s == '=')
	goto doshell;

    for (s = cmd; *s; s++) {
	if (*s != ' ' && !isALPHA(*s) && strchr("$&*(){}[]'\";\\|?<>~`\n",*s)) {
	    if (*s == '\n' && !s[1]) {
		*s = '\0';
		break;
	    }
	  doshell:
	    execl("/bin/sh","sh","-c",cmd,(char*)0);
	    return FALSE;
	}
    }

    New(402,Argv, (s - cmd) / 2 + 2, char*);
    Cmd = savepvn(cmd, s-cmd);
    a = Argv;
    for (s = Cmd; *s;) {
	while (*s && isSPACE(*s)) s++;
	if (*s)
	    *(a++) = s;
	while (*s && !isSPACE(*s)) s++;
	if (*s)
	    *s++ = '\0';
    }
    *a = Nullch;
    if (Argv[0]) {
	execvp(Argv[0],Argv);
	if (errno == ENOEXEC) {		/* for system V NIH syndrome */
	    do_execfree();
	    goto doshell;
	}
	if (dowarn)
	    warn("Can't exec \"%s\": %s", Argv[0], Strerror(errno));
    }
    do_execfree();
    return FALSE;
}

I32
apply(type,mark,sp)
I32 type;
register SV **mark;
register SV **sp;
{
    register I32 val;
    register I32 val2;
    register I32 tot = 0;
    char *s;
    SV **oldmark = mark;

    if (tainting) {
	while (++mark <= sp) {
	    MAGIC *mg;
	    if (SvMAGICAL(*mark) && (mg = mg_find(*mark, 't')) && mg->mg_len & 1)
		tainted = TRUE;
	}
	mark = oldmark;
    }
    switch (type) {
    case OP_CHMOD:
	TAINT_PROPER("chmod");
	if (++mark <= sp) {
	    tot = sp - mark;
	    val = SvIVx(*mark);
	    while (++mark <= sp) {
		if (chmod(SvPVx(*mark, na),val))
		    tot--;
	    }
	}
	break;
#ifdef HAS_CHOWN
    case OP_CHOWN:
	TAINT_PROPER("chown");
	if (sp - mark > 2) {
	    val = SvIVx(*++mark);
	    val2 = SvIVx(*++mark);
	    tot = sp - mark;
	    while (++mark <= sp) {
		if (chown(SvPVx(*mark, na),val,val2))
		    tot--;
	    }
	}
	break;
#endif
#ifdef HAS_KILL
    case OP_KILL:
	TAINT_PROPER("kill");
	s = SvPVx(*++mark, na);
	tot = sp - mark;
	if (isUPPER(*s)) {
	    if (*s == 'S' && s[1] == 'I' && s[2] == 'G')
		s += 3;
	    if (!(val = whichsig(s)))
		croak("Unrecognized signal name \"%s\"",s);
	}
	else
	    val = SvIVx(*mark);
#ifdef VMS
	/* kill() doesn't do process groups (job trees?) under VMS */
	if (val < 0) val = -val;
	if (val == SIGKILL) {
#	    include <starlet.h>
	    /* Use native sys$delprc() to insure that target process is
	     * deleted; supervisor-mode images don't pay attention to
	     * CRTL's emulation of Unix-style signals and kill()
	     */
	    while (++mark <= sp) {
		I32 proc = SvIVx(*mark);
		register unsigned long int __vmssts;
		if (!((__vmssts = sys$delprc(&proc,0)) & 1)) {
		    tot--;
		    switch (__vmssts) {
			case SS$_NONEXPR:
			case SS$_NOSUCHNODE:
			    SETERRNO(ESRCH,__vmssts);
			    break;
			case SS$_NOPRIV:
			    SETERRNO(EPERM,__vmssts);
			    break;
			default:
			    SETERRNO(EVMSERR,__vmssts);
		    }
		}
	    }
	    break;
	}
#endif
	if (val < 0) {
	    val = -val;
	    while (++mark <= sp) {
		I32 proc = SvIVx(*mark);
#ifdef HAS_KILLPG
		if (killpg(proc,val))	/* BSD */
#else
		if (kill(-proc,val))	/* SYSV */
#endif
		    tot--;
	    }
	}
	else {
	    while (++mark <= sp) {
		if (kill(SvIVx(*mark),val))
		    tot--;
	    }
	}
	break;
#endif
    case OP_UNLINK:
	TAINT_PROPER("unlink");
	tot = sp - mark;
	while (++mark <= sp) {
	    s = SvPVx(*mark, na);
	    if (euid || unsafe) {
		if (UNLINK(s))
		    tot--;
	    }
	    else {	/* don't let root wipe out directories without -U */
#ifdef HAS_LSTAT
		if (lstat(s,&statbuf) < 0 || S_ISDIR(statbuf.st_mode))
#else
		if (Stat(s,&statbuf) < 0 || S_ISDIR(statbuf.st_mode))
#endif
		    tot--;
		else {
		    if (UNLINK(s))
			tot--;
		}
	    }
	}
	break;
#ifdef HAS_UTIME
    case OP_UTIME:
	TAINT_PROPER("utime");
	if (sp - mark > 2) {
#if defined(I_UTIME) || defined(VMS)
	    struct utimbuf utbuf;
#else
	    struct {
		long    actime;
		long	modtime;
	    } utbuf;
#endif

	    Zero(&utbuf, sizeof utbuf, char);
	    utbuf.actime = SvIVx(*++mark);    /* time accessed */
	    utbuf.modtime = SvIVx(*++mark);    /* time modified */
	    tot = sp - mark;
	    while (++mark <= sp) {
		if (utime(SvPVx(*mark, na),&utbuf))
		    tot--;
	    }
	}
	else
	    tot = 0;
	break;
#endif
    }
    return tot;
}

/* Do the permissions allow some operation?  Assumes statcache already set. */
#ifndef VMS /* VMS' cando is in vms.c */
I32
cando(bit, effective, statbufp)
I32 bit;
I32 effective;
register struct stat *statbufp;
{
#ifdef DOSISH
    /* [Comments and code from Len Reed]
     * MS-DOS "user" is similar to UNIX's "superuser," but can't write
     * to write-protected files.  The execute permission bit is set
     * by the Miscrosoft C library stat() function for the following:
     *		.exe files
     *		.com files
     *		.bat files
     *		directories
     * All files and directories are readable.
     * Directories and special files, e.g. "CON", cannot be
     * write-protected.
     * [Comment by Tom Dinger -- a directory can have the write-protect
     *		bit set in the file system, but DOS permits changes to
     *		the directory anyway.  In addition, all bets are off
     *		here for networked software, such as Novell and
     *		Sun's PC-NFS.]
     */

     /* Atari stat() does pretty much the same thing. we set x_bit_set_in_stat
      * too so it will actually look into the files for magic numbers
      */
     return (bit & statbufp->st_mode) ? TRUE : FALSE;

#else /* ! MSDOS */
    if ((effective ? euid : uid) == 0) {	/* root is special */
	if (bit == S_IXUSR) {
	    if (statbufp->st_mode & 0111 || S_ISDIR(statbufp->st_mode))
		return TRUE;
	}
	else
	    return TRUE;		/* root reads and writes anything */
	return FALSE;
    }
    if (statbufp->st_uid == (effective ? euid : uid) ) {
	if (statbufp->st_mode & bit)
	    return TRUE;	/* ok as "user" */
    }
    else if (ingroup((I32)statbufp->st_gid,effective)) {
	if (statbufp->st_mode & bit >> 3)
	    return TRUE;	/* ok as "group" */
    }
    else if (statbufp->st_mode & bit >> 6)
	return TRUE;	/* ok as "other" */
    return FALSE;
#endif /* ! MSDOS */
}
#endif /* ! VMS */

I32
ingroup(testgid,effective)
I32 testgid;
I32 effective;
{
    if (testgid == (effective ? egid : gid))
	return TRUE;
#ifdef HAS_GETGROUPS
#ifndef NGROUPS
#define NGROUPS 32
#endif
    {
	Groups_t gary[NGROUPS];
	I32 anum;

	anum = getgroups(NGROUPS,gary);
	while (--anum >= 0)
	    if (gary[anum] == testgid)
		return TRUE;
    }
#endif
    return FALSE;
}

#if defined(HAS_MSG) || defined(HAS_SEM) || defined(HAS_SHM)

I32
do_ipcget(optype, mark, sp)
I32 optype;
SV **mark;
SV **sp;
{
    key_t key;
    I32 n, flags;

    key = (key_t)SvNVx(*++mark);
    n = (optype == OP_MSGGET) ? 0 : SvIVx(*++mark);
    flags = SvIVx(*++mark);
    SETERRNO(0,0);
    switch (optype)
    {
#ifdef HAS_MSG
    case OP_MSGGET:
	return msgget(key, flags);
#endif
#ifdef HAS_SEM
    case OP_SEMGET:
	return semget(key, n, flags);
#endif
#ifdef HAS_SHM
    case OP_SHMGET:
	return shmget(key, n, flags);
#endif
#if !defined(HAS_MSG) || !defined(HAS_SEM) || !defined(HAS_SHM)
    default:
	croak("%s not implemented", op_desc[optype]);
#endif
    }
    return -1;			/* should never happen */
}

I32
do_ipcctl(optype, mark, sp)
I32 optype;
SV **mark;
SV **sp;
{
    SV *astr;
    char *a;
    I32 id, n, cmd, infosize, getinfo;
    I32 ret = -1;

    id = SvIVx(*++mark);
    n = (optype == OP_SEMCTL) ? SvIVx(*++mark) : 0;
    cmd = SvIVx(*++mark);
    astr = *++mark;
    infosize = 0;
    getinfo = (cmd == IPC_STAT);

    switch (optype)
    {
#ifdef HAS_MSG
    case OP_MSGCTL:
	if (cmd == IPC_STAT || cmd == IPC_SET)
	    infosize = sizeof(struct msqid_ds);
	break;
#endif
#ifdef HAS_SHM
    case OP_SHMCTL:
	if (cmd == IPC_STAT || cmd == IPC_SET)
	    infosize = sizeof(struct shmid_ds);
	break;
#endif
#ifdef HAS_SEM
    case OP_SEMCTL:
	if (cmd == IPC_STAT || cmd == IPC_SET)
	    infosize = sizeof(struct semid_ds);
	else if (cmd == GETALL || cmd == SETALL)
	{
	    struct semid_ds semds;
	    if (semctl(id, 0, IPC_STAT, &semds) == -1)
		return -1;
	    getinfo = (cmd == GETALL);
	    infosize = semds.sem_nsems * sizeof(short);
		/* "short" is technically wrong but much more portable
		   than guessing about u_?short(_t)? */
	}
	break;
#endif
#if !defined(HAS_MSG) || !defined(HAS_SEM) || !defined(HAS_SHM)
    default:
	croak("%s not implemented", op_desc[optype]);
#endif
    }

    if (infosize)
    {
	STRLEN len;
	if (getinfo)
	{
	    SvPV_force(astr, len);
	    a = SvGROW(astr, infosize+1);
	}
	else
	{
	    a = SvPV(astr, len);
	    if (len != infosize)
		croak("Bad arg length for %s, is %d, should be %d",
			op_desc[optype], len, infosize);
	}
    }
    else
    {
	I32 i = SvIV(astr);
	a = (char *)i;		/* ouch */
    }
    SETERRNO(0,0);
    switch (optype)
    {
#ifdef HAS_MSG
    case OP_MSGCTL:
	ret = msgctl(id, cmd, (struct msqid_ds *)a);
	break;
#endif
#ifdef HAS_SEM
    case OP_SEMCTL:
	ret = semctl(id, n, cmd, (struct semid_ds *)a);
	break;
#endif
#ifdef HAS_SHM
    case OP_SHMCTL:
	ret = shmctl(id, cmd, (struct shmid_ds *)a);
	break;
#endif
    }
    if (getinfo && ret >= 0) {
	SvCUR_set(astr, infosize);
	*SvEND(astr) = '\0';
	SvSETMAGIC(astr);
    }
    return ret;
}

I32
do_msgsnd(mark, sp)
SV **mark;
SV **sp;
{
#ifdef HAS_MSG
    SV *mstr;
    char *mbuf;
    I32 id, msize, flags;
    STRLEN len;

    id = SvIVx(*++mark);
    mstr = *++mark;
    flags = SvIVx(*++mark);
    mbuf = SvPV(mstr, len);
    if ((msize = len - sizeof(long)) < 0)
	croak("Arg too short for msgsnd");
    SETERRNO(0,0);
    return msgsnd(id, (struct msgbuf *)mbuf, msize, flags);
#else
    croak("msgsnd not implemented");
#endif
}

I32
do_msgrcv(mark, sp)
SV **mark;
SV **sp;
{
#ifdef HAS_MSG
    SV *mstr;
    char *mbuf;
    long mtype;
    I32 id, msize, flags, ret;
    STRLEN len;

    id = SvIVx(*++mark);
    mstr = *++mark;
    msize = SvIVx(*++mark);
    mtype = (long)SvIVx(*++mark);
    flags = SvIVx(*++mark);
    if (SvTHINKFIRST(mstr)) {
	if (SvREADONLY(mstr))
	    croak("Can't msgrcv to readonly var");
	if (SvROK(mstr))
	    sv_unref(mstr);
    }
    SvPV_force(mstr, len);
    mbuf = SvGROW(mstr, sizeof(long)+msize+1);
    
    SETERRNO(0,0);
    ret = msgrcv(id, (struct msgbuf *)mbuf, msize, mtype, flags);
    if (ret >= 0) {
	SvCUR_set(mstr, sizeof(long)+ret);
	*SvEND(mstr) = '\0';
    }
    return ret;
#else
    croak("msgrcv not implemented");
#endif
}

I32
do_semop(mark, sp)
SV **mark;
SV **sp;
{
#ifdef HAS_SEM
    SV *opstr;
    char *opbuf;
    I32 id;
    STRLEN opsize;

    id = SvIVx(*++mark);
    opstr = *++mark;
    opbuf = SvPV(opstr, opsize);
    if (opsize < sizeof(struct sembuf)
	|| (opsize % sizeof(struct sembuf)) != 0) {
	SETERRNO(EINVAL,LIB$_INVARG);
	return -1;
    }
    SETERRNO(0,0);
    return semop(id, (struct sembuf *)opbuf, opsize/sizeof(struct sembuf));
#else
    croak("semop not implemented");
#endif
}

I32
do_shmio(optype, mark, sp)
I32 optype;
SV **mark;
SV **sp;
{
#ifdef HAS_SHM
    SV *mstr;
    char *mbuf, *shm;
    I32 id, mpos, msize;
    STRLEN len;
    struct shmid_ds shmds;

    id = SvIVx(*++mark);
    mstr = *++mark;
    mpos = SvIVx(*++mark);
    msize = SvIVx(*++mark);
    SETERRNO(0,0);
    if (shmctl(id, IPC_STAT, &shmds) == -1)
	return -1;
    if (mpos < 0 || msize < 0 || mpos + msize > shmds.shm_segsz) {
	SETERRNO(EFAULT,SS$_ACCVIO);		/* can't do as caller requested */
	return -1;
    }
    shm = (Shmat_t)shmat(id, (char*)NULL, (optype == OP_SHMREAD) ? SHM_RDONLY : 0);
    if (shm == (char *)-1)	/* I hate System V IPC, I really do */
	return -1;
    if (optype == OP_SHMREAD) {
	SvPV_force(mstr, len);
	mbuf = SvGROW(mstr, msize+1);

	Copy(shm + mpos, mbuf, msize, char);
	SvCUR_set(mstr, msize);
	*SvEND(mstr) = '\0';
	SvSETMAGIC(mstr);
    }
    else {
	I32 n;

	mbuf = SvPV(mstr, len);
	if ((n = len) > msize)
	    n = msize;
	Copy(mbuf, shm + mpos, n, char);
	if (n < msize)
	    memzero(shm + mpos + n, msize - n);
    }
    return shmdt(shm);
#else
    croak("shm I/O not implemented");
#endif
}

#endif /* SYSV IPC */
