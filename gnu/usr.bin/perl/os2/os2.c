#define INCL_DOS
#define INCL_NOPM
#define INCL_DOSFILEMGR
#define INCL_DOSMEMMGR
#define INCL_DOSERRORS
#include <os2.h>

/*
 * Various Unix compatibility functions for OS/2
 */

#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <process.h>
#include <fcntl.h>

#include "EXTERN.h"
#include "perl.h"

/*****************************************************************************/
/* 2.1 would not resolve symbols on demand, and has no ExtLIBPATH. */
static PFN ExtFCN[2];			/* Labeled by ord below. */
static USHORT loadOrd[2] = { 874, 873 }; /* Query=874, Set=873. */
#define ORD_QUERY_ELP	0
#define ORD_SET_ELP	1

APIRET
loadByOrd(ULONG ord)
{
    if (ExtFCN[ord] == NULL) {
	static HMODULE hdosc = 0;
	BYTE buf[20];
	PFN fcn;
	APIRET rc;

	if ((!hdosc && CheckOSError(DosLoadModule(buf, sizeof buf, 
						  "doscalls", &hdosc)))
	    || CheckOSError(DosQueryProcAddr(hdosc, loadOrd[ord], NULL, &fcn)))
	    die("This version of OS/2 does not support doscalls.%i", 
		loadOrd[ord]);
	ExtFCN[ord] = fcn;
    } 
    if ((long)ExtFCN[ord] == -1) die("panic queryaddr");
}

/* priorities */
static signed char priors[] = {0, 1, 3, 2}; /* Last two interchanged,
					       self inverse. */
#define QSS_INI_BUFFER 1024

PQTOPLEVEL
get_sysinfo(ULONG pid, ULONG flags)
{
    char *pbuffer;
    ULONG rc, buf_len = QSS_INI_BUFFER;

    New(1322, pbuffer, buf_len, char);
    /* QSS_PROCESS | QSS_MODULE | QSS_SEMAPHORES | QSS_SHARED */
    rc = QuerySysState(flags, pid, pbuffer, buf_len);
    while (rc == ERROR_BUFFER_OVERFLOW) {
	Renew(pbuffer, buf_len *= 2, char);
	rc = QuerySysState(flags, pid, pbuffer, buf_len);
    }
    if (rc) {
	FillOSError(rc);
	Safefree(pbuffer);
	return 0;
    }
    return (PQTOPLEVEL)pbuffer;
}

#define PRIO_ERR 0x1111

static ULONG
sys_prio(pid)
{
  ULONG prio;
  PQTOPLEVEL psi;

  psi = get_sysinfo(pid, QSS_PROCESS);
  if (!psi) {
      return PRIO_ERR;
  }
  if (pid != psi->procdata->pid) {
      Safefree(psi);
      croak("panic: wrong pid in sysinfo");
  }
  prio = psi->procdata->threads->priority;
  Safefree(psi);
  return prio;
}

int 
setpriority(int which, int pid, int val)
{
  ULONG rc, prio;
  PQTOPLEVEL psi;

  prio = sys_prio(pid);

  if (!(_emx_env & 0x200)) return 0; /* Nop if not OS/2. */
  if (priors[(32 - val) >> 5] + 1 == (prio >> 8)) {
      /* Do not change class. */
      return CheckOSError(DosSetPriority((pid < 0) 
					 ? PRTYS_PROCESSTREE : PRTYS_PROCESS,
					 0, 
					 (32 - val) % 32 - (prio & 0xFF), 
					 abs(pid)))
      ? -1 : 0;
  } else /* if ((32 - val) % 32 == (prio & 0xFF)) */ {
      /* Documentation claims one can change both class and basevalue,
       * but I find it wrong. */
      /* Change class, but since delta == 0 denotes absolute 0, correct. */
      if (CheckOSError(DosSetPriority((pid < 0) 
				      ? PRTYS_PROCESSTREE : PRTYS_PROCESS,
				      priors[(32 - val) >> 5] + 1, 
				      0, 
				      abs(pid)))) 
	  return -1;
      if ( ((32 - val) % 32) == 0 ) return 0;
      return CheckOSError(DosSetPriority((pid < 0) 
					 ? PRTYS_PROCESSTREE : PRTYS_PROCESS,
					 0, 
					 (32 - val) % 32, 
					 abs(pid)))
	  ? -1 : 0;
  } 
/*   else return CheckOSError(DosSetPriority((pid < 0)  */
/* 					  ? PRTYS_PROCESSTREE : PRTYS_PROCESS, */
/* 					  priors[(32 - val) >> 5] + 1,  */
/* 					  (32 - val) % 32 - (prio & 0xFF),  */
/* 					  abs(pid))) */
/*       ? -1 : 0; */
}

int 
getpriority(int which /* ignored */, int pid)
{
  TIB *tib;
  PIB *pib;
  ULONG rc, ret;

  if (!(_emx_env & 0x200)) return 0; /* Nop if not OS/2. */
  /* DosGetInfoBlocks has old priority! */
/*   if (CheckOSError(DosGetInfoBlocks(&tib, &pib))) return -1; */
/*   if (pid != pib->pib_ulpid) { */
  ret = sys_prio(pid);
  if (ret == PRIO_ERR) {
      return -1;
  }
/*   } else */
/*       ret = tib->tib_ptib2->tib2_ulpri; */
  return (1 - priors[((ret >> 8) - 1)])*32 - (ret & 0xFF);
}

/*****************************************************************************/
/* spawn */
typedef void (*Sigfunc) _((int));

static int
result(int flag, int pid)
{
	int r, status;
	Signal_t (*ihand)();     /* place to save signal during system() */
	Signal_t (*qhand)();     /* place to save signal during system() */
#ifndef __EMX__
	RESULTCODES res;
	int rpid;
#endif

	if (pid < 0 || flag != 0)
		return pid;

#ifdef __EMX__
	ihand = rsignal(SIGINT, SIG_IGN);
	qhand = rsignal(SIGQUIT, SIG_IGN);
	do {
	    r = wait4pid(pid, &status, 0);
	} while (r == -1 && errno == EINTR);
	rsignal(SIGINT, ihand);
	rsignal(SIGQUIT, qhand);

	statusvalue = (U16)status;
	if (r < 0)
		return -1;
	return status & 0xFFFF;
#else
	ihand = rsignal(SIGINT, SIG_IGN);
	r = DosWaitChild(DCWA_PROCESS, DCWW_WAIT, &res, &rpid, pid);
	rsignal(SIGINT, ihand);
	statusvalue = res.codeResult << 8 | res.codeTerminate;
	if (r)
		return -1;
	return statusvalue;
#endif
}

int
do_aspawn(really,mark,sp)
SV *really;
register SV **mark;
register SV **sp;
{
    register char **a;
    char *tmps = NULL;
    int rc;
    int flag = P_WAIT, trueflag, err, secondtry = 0;

    if (sp > mark) {
	New(1301,Argv, sp - mark + 3, char*);
	a = Argv;

	if (mark < sp && SvNIOKp(*(mark+1)) && !SvPOKp(*(mark+1))) {
		++mark;
		flag = SvIVx(*mark);
	}

	while (++mark <= sp) {
	    if (*mark)
		*a++ = SvPVx(*mark, na);
	    else
		*a++ = "";
	}
	*a = Nullch;

	trueflag = flag;
	if (flag == P_WAIT)
		flag = P_NOWAIT;

	if (strEQ(Argv[0],"/bin/sh")) Argv[0] = sh_path;

	if (Argv[0][0] != '/' && Argv[0][0] != '\\'
	    && !(Argv[0][0] && Argv[0][1] == ':' 
		 && (Argv[0][2] == '/' || Argv[0][2] != '\\'))
	    ) /* will swawnvp use PATH? */
	    TAINT_ENV();	/* testing IFS here is overkill, probably */
	/* We should check PERL_SH* and PERLLIB_* as well? */
      retry:
	if (really && *(tmps = SvPV(really, na)))
	    rc = result(trueflag, spawnvp(flag,tmps,Argv));
	else
	    rc = result(trueflag, spawnvp(flag,Argv[0],Argv));

	if (rc < 0 && secondtry == 0 
	    && (!tmps || !*tmps)) { /* Cannot transfer `really' via shell. */
	    err = errno;
	    if (err == ENOENT) {	/* No such file. */
		/* One reason may be that EMX added .exe. We suppose
		   that .exe-less files are automatically shellable. */
		char *no_dir;
		(no_dir = strrchr(Argv[0], '/')) 
		    || (no_dir = strrchr(Argv[0], '\\'))
		    || (no_dir = Argv[0]);
		if (!strchr(no_dir, '.')) {
		    struct stat buffer;
		    if (stat(Argv[0], &buffer) != -1) { /* File exists. */
			/* Maybe we need to specify the full name here? */
			goto doshell;
		    }
		}
	    } else if (err == ENOEXEC) { /* Need to send to shell. */
	      doshell:
		while (a >= Argv) {
		    *(a + 2) = *a;
		    a--;
		}
		*Argv = sh_path;
		*(Argv + 1) = "-c";
		secondtry = 1;
		goto retry;
	    }
	}
	if (rc < 0 && dowarn)
	    warn("Can't spawn \"%s\": %s", Argv[0], Strerror(errno));
	if (rc < 0) rc = 255 << 8; /* Emulate the fork(). */
    } else
    	rc = -1;
    do_execfree();
    return rc;
}

#define EXECF_SPAWN 0
#define EXECF_EXEC 1
#define EXECF_TRUEEXEC 2
#define EXECF_SPAWN_NOWAIT 3

int
do_spawn2(cmd, execf)
char *cmd;
int execf;
{
    register char **a;
    register char *s;
    char flags[10];
    char *shell, *copt, *news = NULL;
    int rc, added_shell = 0, err, seenspace = 0;
    char fullcmd[MAXNAMLEN + 1];

#ifdef TRYSHELL
    if ((shell = getenv("EMXSHELL")) != NULL)
    	copt = "-c";
    else if ((shell = getenv("SHELL")) != NULL)
    	copt = "-c";
    else if ((shell = getenv("COMSPEC")) != NULL)
    	copt = "/C";
    else
    	shell = "cmd.exe";
#else
    /* Consensus on perl5-porters is that it is _very_ important to
       have a shell which will not change between computers with the
       same architecture, to avoid "action on a distance". 
       And to have simple build, this shell should be sh. */
    shell = sh_path;
    copt = "-c";
#endif 

    while (*cmd && isSPACE(*cmd))
	cmd++;

    if (strnEQ(cmd,"/bin/sh",7) && isSPACE(cmd[7])) {
	STRLEN l = strlen(sh_path);
	
	New(1302, news, strlen(cmd) - 7 + l + 1, char);
	strcpy(news, sh_path);
	strcpy(news + l, cmd + 7);
	cmd = news;
	added_shell = 1;
    }

    /* save an extra exec if possible */
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
	    if (*s == '\n' && s[1] == '\0') {
		*s = '\0';
		break;
	    } else if (*s == '\\' && !seenspace) {
		continue;		/* Allow backslashes in names */
	    }
	  doshell:
	    if (execf == EXECF_TRUEEXEC)
                return execl(shell,shell,copt,cmd,(char*)0);
	    else if (execf == EXECF_EXEC)
                return spawnl(P_OVERLAY,shell,shell,copt,cmd,(char*)0);
	    else if (execf == EXECF_SPAWN_NOWAIT)
                return spawnl(P_NOWAIT,shell,shell,copt,cmd,(char*)0);
	    /* In the ak code internal P_NOWAIT is P_WAIT ??? */
	    rc = result(P_WAIT,
			spawnl(P_NOWAIT,shell,shell,copt,cmd,(char*)0));
	    if (rc < 0 && dowarn)
		warn("Can't %s \"%s\": %s", 
		     (execf == EXECF_SPAWN ? "spawn" : "exec"),
		     shell, Strerror(errno));
	    if (rc < 0) rc = 255 << 8; /* Emulate the fork(). */
	    if (news) Safefree(news);
	    return rc;
	} else if (*s == ' ' || *s == '\t') {
	    seenspace = 1;
	}
    }

    New(1303,Argv, (s - cmd) / 2 + 2, char*);
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
	int err;
	
	if (execf == EXECF_TRUEEXEC)
	    rc = execvp(Argv[0],Argv);
	else if (execf == EXECF_EXEC)
	    rc = spawnvp(P_OVERLAY,Argv[0],Argv);
	else if (execf == EXECF_SPAWN_NOWAIT)
	    rc = spawnvp(P_NOWAIT,Argv[0],Argv);
        else
	    rc = result(P_WAIT, spawnvp(P_NOWAIT,Argv[0],Argv));
	if (rc < 0) {
	    err = errno;
	    if (err == ENOENT) {	/* No such file. */
		/* One reason may be that EMX added .exe. We suppose
		   that .exe-less files are automatically shellable. */
		char *no_dir;
		(no_dir = strrchr(Argv[0], '/')) 
		    || (no_dir = strrchr(Argv[0], '\\'))
		    || (no_dir = Argv[0]);
		if (!strchr(no_dir, '.')) {
		    struct stat buffer;
		    if (stat(Argv[0], &buffer) != -1) { /* File exists. */
			/* Maybe we need to specify the full name here? */
			goto doshell;
		    }
		}
	    } else if (err == ENOEXEC) { /* Need to send to shell. */
		goto doshell;
	    }
	}
	if (rc < 0 && dowarn)
	    warn("Can't %s \"%s\": %s", 
		 ((execf != EXECF_EXEC && execf != EXECF_TRUEEXEC) 
		  ? "spawn" : "exec"),
		 Argv[0], Strerror(err));
	if (rc < 0) rc = 255 << 8; /* Emulate the fork(). */
    } else
    	rc = -1;
    if (news) Safefree(news);
    do_execfree();
    return rc;
}

int
do_spawn(cmd)
char *cmd;
{
    return do_spawn2(cmd, EXECF_SPAWN);
}

int
do_spawn_nowait(cmd)
char *cmd;
{
    return do_spawn2(cmd, EXECF_SPAWN_NOWAIT);
}

bool
do_exec(cmd)
char *cmd;
{
    return do_spawn2(cmd, EXECF_EXEC);
}

bool
os2exec(cmd)
char *cmd;
{
    return do_spawn2(cmd, EXECF_TRUEEXEC);
}

PerlIO *
my_syspopen(cmd,mode)
char	*cmd;
char	*mode;
{
#ifndef USE_POPEN

    int p[2];
    register I32 this, that, newfd;
    register I32 pid, rc;
    PerlIO *res;
    SV *sv;
    
    if (pipe(p) < 0)
	return Nullfp;
    /* `this' is what we use in the parent, `that' in the child. */
    this = (*mode == 'w');
    that = !this;
    if (tainting) {
	taint_env();
	taint_proper("Insecure %s%s", "EXEC");
    }
    /* Now we need to spawn the child. */
    newfd = dup(*mode == 'r');		/* Preserve std* */
    if (p[that] != (*mode == 'r')) {
	dup2(p[that], *mode == 'r');
	close(p[that]);
    }
    /* Where is `this' and newfd now? */
    fcntl(p[this], F_SETFD, FD_CLOEXEC);
    fcntl(newfd, F_SETFD, FD_CLOEXEC);
    pid = do_spawn_nowait(cmd);
    if (newfd != (*mode == 'r')) {
	dup2(newfd, *mode == 'r');	/* Return std* back. */
	close(newfd);
    }
    close(p[that]);
    if (pid == -1) {
	close(p[this]);
	return NULL;
    }
    if (p[that] < p[this]) {
	dup2(p[this], p[that]);
	close(p[this]);
	p[this] = p[that];
    }
    sv = *av_fetch(fdpid,p[this],TRUE);
    (void)SvUPGRADE(sv,SVt_IV);
    SvIVX(sv) = pid;
    forkprocess = pid;
    return PerlIO_fdopen(p[this], mode);

#else  /* USE_POPEN */

    PerlIO *res;
    SV *sv;

#  ifdef TRYSHELL
    res = popen(cmd, mode);
#  else
    char *shell = getenv("EMXSHELL");

    my_setenv("EMXSHELL", sh_path);
    res = popen(cmd, mode);
    my_setenv("EMXSHELL", shell);
#  endif 
    sv = *av_fetch(fdpid, PerlIO_fileno(res), TRUE);
    (void)SvUPGRADE(sv,SVt_IV);
    SvIVX(sv) = -1;			/* A cooky. */
    return res;

#endif /* USE_POPEN */

}

/******************************************************************/

#ifndef HAS_FORK
int
fork(void)
{
    die(no_func, "Unsupported function fork");
    errno = EINVAL;
    return -1;
}
#endif

/*******************************************************************/
/* not implemented in EMX 0.9a */

void *	ctermid(x)	{ return 0; }

#ifdef MYTTYNAME /* was not in emx0.9a */
void *	ttyname(x)	{ return 0; }
#endif

/******************************************************************/
/* my socket forwarders - EMX lib only provides static forwarders */

static HMODULE htcp = 0;

static void *
tcp0(char *name)
{
    static BYTE buf[20];
    PFN fcn;

    if (!(_emx_env & 0x200)) croak("%s requires OS/2", name); /* Die if not OS/2. */
    if (!htcp)
	DosLoadModule(buf, sizeof buf, "tcp32dll", &htcp);
    if (htcp && DosQueryProcAddr(htcp, 0, name, &fcn) == 0)
	return (void *) ((void * (*)(void)) fcn) ();
    return 0;
}

static void
tcp1(char *name, int arg)
{
    static BYTE buf[20];
    PFN fcn;

    if (!(_emx_env & 0x200)) croak("%s requires OS/2", name); /* Die if not OS/2. */
    if (!htcp)
	DosLoadModule(buf, sizeof buf, "tcp32dll", &htcp);
    if (htcp && DosQueryProcAddr(htcp, 0, name, &fcn) == 0)
	((void (*)(int)) fcn) (arg);
}

void *	gethostent()	{ return tcp0("GETHOSTENT");  }
void *	getnetent()	{ return tcp0("GETNETENT");   }
void *	getprotoent()	{ return tcp0("GETPROTOENT"); }
void *	getservent()	{ return tcp0("GETSERVENT");  }
void	sethostent(x)	{ tcp1("SETHOSTENT",  x); }
void	setnetent(x)	{ tcp1("SETNETENT",   x); }
void	setprotoent(x)	{ tcp1("SETPROTOENT", x); }
void	setservent(x)	{ tcp1("SETSERVENT",  x); }
void	endhostent()	{ tcp0("ENDHOSTENT");  }
void	endnetent()	{ tcp0("ENDNETENT");   }
void	endprotoent()	{ tcp0("ENDPROTOENT"); }
void	endservent()	{ tcp0("ENDSERVENT");  }

/*****************************************************************************/
/* not implemented in C Set++ */

#ifndef __EMX__
int	setuid(x)	{ errno = EINVAL; return -1; }
int	setgid(x)	{ errno = EINVAL; return -1; }
#endif

/*****************************************************************************/
/* stat() hack for char/block device */

#if OS2_STAT_HACK

    /* First attempt used DosQueryFSAttach which crashed the system when
       used with 5.001. Now just look for /dev/. */

int
os2_stat(char *name, struct stat *st)
{
    static int ino = SHRT_MAX;

    if (stricmp(name, "/dev/con") != 0
     && stricmp(name, "/dev/tty") != 0)
	return stat(name, st);

    memset(st, 0, sizeof *st);
    st->st_mode = S_IFCHR|0666;
    st->st_ino = (ino-- & 0x7FFF);
    st->st_nlink = 1;
    return 0;
}

#endif

#ifdef USE_PERL_SBRK

/* SBRK() emulation, mostly moved to malloc.c. */

void *
sys_alloc(int size) {
    void *got;
    APIRET rc = DosAllocMem(&got, size, PAG_COMMIT | PAG_WRITE);

    if (rc == ERROR_NOT_ENOUGH_MEMORY) {
	return (void *) -1;
    } else if ( rc ) die("Got an error from DosAllocMem: %li", (long)rc);
    return got;
}

#endif /* USE_PERL_SBRK */

/* tmp path */

char *tmppath = TMPPATH1;

void
settmppath()
{
    char *p = getenv("TMP"), *tpath;
    int len;

    if (!p) p = getenv("TEMP");
    if (!p) return;
    len = strlen(p);
    tpath = (char *)malloc(len + strlen(TMPPATH1) + 2);
    strcpy(tpath, p);
    tpath[len] = '/';
    strcpy(tpath + len + 1, TMPPATH1);
    tmppath = tpath;
}

#include "XSUB.h"

XS(XS_File__Copy_syscopy)
{
    dXSARGS;
    if (items < 2 || items > 3)
	croak("Usage: File::Copy::syscopy(src,dst,flag=0)");
    {
	char *	src = (char *)SvPV(ST(0),na);
	char *	dst = (char *)SvPV(ST(1),na);
	U32	flag;
	int	RETVAL, rc;

	if (items < 3)
	    flag = 0;
	else {
	    flag = (unsigned long)SvIV(ST(2));
	}

	RETVAL = !CheckOSError(DosCopy(src, dst, flag));
	ST(0) = sv_newmortal();
	sv_setiv(ST(0), (IV)RETVAL);
    }
    XSRETURN(1);
}

char *
mod2fname(sv)
     SV   *sv;
{
    static char fname[9];
    int pos = 6, len, avlen;
    unsigned int sum = 0;
    AV  *av;
    SV  *svp;
    char *s;

    if (!SvROK(sv)) croak("Not a reference given to mod2fname");
    sv = SvRV(sv);
    if (SvTYPE(sv) != SVt_PVAV) 
      croak("Not array reference given to mod2fname");

    avlen = av_len((AV*)sv);
    if (avlen < 0) 
      croak("Empty array reference given to mod2fname");

    s = SvPV(*av_fetch((AV*)sv, avlen, FALSE), na);
    strncpy(fname, s, 8);
    len = strlen(s);
    if (len < 6) pos = len;
    while (*s) {
	sum = 33 * sum + *(s++);	/* Checksumming first chars to
					 * get the capitalization into c.s. */
    }
    avlen --;
    while (avlen >= 0) {
	s = SvPV(*av_fetch((AV*)sv, avlen, FALSE), na);
	while (*s) {
	    sum = 33 * sum + *(s++);	/* 7 is primitive mod 13. */
	}
	avlen --;
    }
    fname[pos] = 'A' + (sum % 26);
    fname[pos + 1] = 'A' + (sum / 26 % 26);
    fname[pos + 2] = '\0';
    return (char *)fname;
}

XS(XS_DynaLoader_mod2fname)
{
    dXSARGS;
    if (items != 1)
	croak("Usage: DynaLoader::mod2fname(sv)");
    {
	SV *	sv = ST(0);
	char *	RETVAL;

	RETVAL = mod2fname(sv);
	ST(0) = sv_newmortal();
	sv_setpv((SV*)ST(0), RETVAL);
    }
    XSRETURN(1);
}

char *
os2error(int rc)
{
	static char buf[300];
	ULONG len;

        if (!(_emx_env & 0x200)) return ""; /* Nop if not OS/2. */
	if (rc == 0)
		return NULL;
	if (DosGetMessage(NULL, 0, buf, sizeof buf - 1, rc, "OSO001.MSG", &len))
		sprintf(buf, "OS/2 system error code %d=0x%x", rc, rc);
	else
		buf[len] = '\0';
	return buf;
}

char *
perllib_mangle(char *s, unsigned int l)
{
    static char *newp, *oldp;
    static int newl, oldl, notfound;
    static char ret[STATIC_FILE_LENGTH+1];
    
    if (!newp && !notfound) {
	newp = getenv("PERLLIB_PREFIX");
	if (newp) {
	    char *s;
	    
	    oldp = newp;
	    while (*newp && !isSPACE(*newp) && *newp != ';') {
		newp++; oldl++;		/* Skip digits. */
	    }
	    while (*newp && (isSPACE(*newp) || *newp == ';')) {
		newp++;			/* Skip whitespace. */
	    }
	    newl = strlen(newp);
	    if (newl == 0 || oldl == 0) {
		die("Malformed PERLLIB_PREFIX");
	    }
	    strcpy(ret, newp);
	    s = ret;
	    while (*s) {
		if (*s == '\\') *s = '/';
		s++;
	    }
	} else {
	    notfound = 1;
	}
    }
    if (!newp) {
	return s;
    }
    if (l == 0) {
	l = strlen(s);
    }
    if (l < oldl || strnicmp(oldp, s, oldl) != 0) {
	return s;
    }
    if (l + newl - oldl > STATIC_FILE_LENGTH || newl > STATIC_FILE_LENGTH) {
	die("Malformed PERLLIB_PREFIX");
    }
    strcpy(ret + newl, s + oldl);
    return ret;
}

extern void dlopen();
void *fakedl = &dlopen;		/* Pull in dynaloading part. */

#define sys_is_absolute(path) ( isALPHA((path)[0]) && (path)[1] == ':' \
				&& ((path)[2] == '/' || (path)[2] == '\\'))
#define sys_is_rooted _fnisabs
#define sys_is_relative _fnisrel
#define current_drive _getdrive

#undef chdir				/* Was _chdir2. */
#define sys_chdir(p) (chdir(p) == 0)
#define change_drive(d) (_chdrive(d), (current_drive() == toupper(d)))

XS(XS_Cwd_current_drive)
{
    dXSARGS;
    if (items != 0)
	croak("Usage: Cwd::current_drive()");
    {
	char	RETVAL;

	RETVAL = current_drive();
	ST(0) = sv_newmortal();
	sv_setpvn(ST(0), (char *)&RETVAL, 1);
    }
    XSRETURN(1);
}

XS(XS_Cwd_sys_chdir)
{
    dXSARGS;
    if (items != 1)
	croak("Usage: Cwd::sys_chdir(path)");
    {
	char *	path = (char *)SvPV(ST(0),na);
	bool	RETVAL;

	RETVAL = sys_chdir(path);
	ST(0) = boolSV(RETVAL);
	if (SvREFCNT(ST(0))) sv_2mortal(ST(0));
    }
    XSRETURN(1);
}

XS(XS_Cwd_change_drive)
{
    dXSARGS;
    if (items != 1)
	croak("Usage: Cwd::change_drive(d)");
    {
	char	d = (char)*SvPV(ST(0),na);
	bool	RETVAL;

	RETVAL = change_drive(d);
	ST(0) = boolSV(RETVAL);
	if (SvREFCNT(ST(0))) sv_2mortal(ST(0));
    }
    XSRETURN(1);
}

XS(XS_Cwd_sys_is_absolute)
{
    dXSARGS;
    if (items != 1)
	croak("Usage: Cwd::sys_is_absolute(path)");
    {
	char *	path = (char *)SvPV(ST(0),na);
	bool	RETVAL;

	RETVAL = sys_is_absolute(path);
	ST(0) = boolSV(RETVAL);
	if (SvREFCNT(ST(0))) sv_2mortal(ST(0));
    }
    XSRETURN(1);
}

XS(XS_Cwd_sys_is_rooted)
{
    dXSARGS;
    if (items != 1)
	croak("Usage: Cwd::sys_is_rooted(path)");
    {
	char *	path = (char *)SvPV(ST(0),na);
	bool	RETVAL;

	RETVAL = sys_is_rooted(path);
	ST(0) = boolSV(RETVAL);
	if (SvREFCNT(ST(0))) sv_2mortal(ST(0));
    }
    XSRETURN(1);
}

XS(XS_Cwd_sys_is_relative)
{
    dXSARGS;
    if (items != 1)
	croak("Usage: Cwd::sys_is_relative(path)");
    {
	char *	path = (char *)SvPV(ST(0),na);
	bool	RETVAL;

	RETVAL = sys_is_relative(path);
	ST(0) = boolSV(RETVAL);
	if (SvREFCNT(ST(0))) sv_2mortal(ST(0));
    }
    XSRETURN(1);
}

XS(XS_Cwd_sys_cwd)
{
    dXSARGS;
    if (items != 0)
	croak("Usage: Cwd::sys_cwd()");
    {
	char p[MAXPATHLEN];
	char *	RETVAL;
	RETVAL = _getcwd2(p, MAXPATHLEN);
	ST(0) = sv_newmortal();
	sv_setpv((SV*)ST(0), RETVAL);
    }
    XSRETURN(1);
}

XS(XS_Cwd_sys_abspath)
{
    dXSARGS;
    if (items < 1 || items > 2)
	croak("Usage: Cwd::sys_abspath(path, dir = NULL)");
    {
	char *	path = (char *)SvPV(ST(0),na);
	char *	dir;
	char p[MAXPATHLEN];
	char *	RETVAL;

	if (items < 2)
	    dir = NULL;
	else {
	    dir = (char *)SvPV(ST(1),na);
	}
	if (path[0] == '.' && (path[1] == '/' || path[1] == '\\')) {
	    path += 2;
	}
	if (dir == NULL) {
	    if (_abspath(p, path, MAXPATHLEN) == 0) {
		RETVAL = p;
	    } else {
		RETVAL = NULL;
	    }
	} else {
	    /* Absolute with drive: */
	    if ( sys_is_absolute(path) ) {
		if (_abspath(p, path, MAXPATHLEN) == 0) {
		    RETVAL = p;
		} else {
		    RETVAL = NULL;
		}
	    } else if (path[0] == '/' || path[0] == '\\') {
		/* Rooted, but maybe on different drive. */
		if (isALPHA(dir[0]) && dir[1] == ':' ) {
		    char p1[MAXPATHLEN];

		    /* Need to prepend the drive. */
		    p1[0] = dir[0];
		    p1[1] = dir[1];
		    Copy(path, p1 + 2, strlen(path) + 1, char);
		    RETVAL = p;
		    if (_abspath(p, p1, MAXPATHLEN) == 0) {
			RETVAL = p;
		    } else {
			RETVAL = NULL;
		    }
		} else if (_abspath(p, path, MAXPATHLEN) == 0) {
		    RETVAL = p;
		} else {
		    RETVAL = NULL;
		}
	    } else {
		/* Either path is relative, or starts with a drive letter. */
		/* If the path starts with a drive letter, then dir is
		   relevant only if 
		   a/b)	it is absolute/x:relative on the same drive.  
		   c)	path is on current drive, and dir is rooted
		   In all the cases it is safe to drop the drive part
		   of the path. */
		if ( !sys_is_relative(path) ) {
		    int is_drived;

		    if ( ( ( sys_is_absolute(dir)
			     || (isALPHA(dir[0]) && dir[1] == ':' 
				 && strnicmp(dir, path,1) == 0)) 
			   && strnicmp(dir, path,1) == 0)
			 || ( !(isALPHA(dir[0]) && dir[1] == ':')
			      && toupper(path[0]) == current_drive())) {
			path += 2;
		    } else if (_abspath(p, path, MAXPATHLEN) == 0) {
			RETVAL = p; goto done;
		    } else {
			RETVAL = NULL; goto done;
		    }
		}
		{
		    /* Need to prepend the absolute path of dir. */
		    char p1[MAXPATHLEN];

		    if (_abspath(p1, dir, MAXPATHLEN) == 0) {
			int l = strlen(p1);

			if (p1[ l - 1 ] != '/') {
			    p1[ l ] = '/';
			    l++;
			}
			Copy(path, p1 + l, strlen(path) + 1, char);
			if (_abspath(p, p1, MAXPATHLEN) == 0) {
			    RETVAL = p;
			} else {
			    RETVAL = NULL;
			}
		    } else {
			RETVAL = NULL;
		    }
		}
	      done:
	    }
	}
	ST(0) = sv_newmortal();
	sv_setpv((SV*)ST(0), RETVAL);
    }
    XSRETURN(1);
}
typedef APIRET (*PELP)(PSZ path, ULONG type);

APIRET
ExtLIBPATH(ULONG ord, PSZ path, ULONG type)
{
    loadByOrd(ord);			/* Guarantied to load or die! */
    return (*(PELP)ExtFCN[ord])(path, type);
}

#define extLibpath(type) 						\
    (CheckOSError(ExtLIBPATH(ORD_QUERY_ELP, to, ((type) ? END_LIBPATH	\
						 : BEGIN_LIBPATH)))	\
     ? NULL : to )

#define extLibpath_set(p,type) 					\
    (!CheckOSError(ExtLIBPATH(ORD_SET_ELP, (p), ((type) ? END_LIBPATH	\
						 : BEGIN_LIBPATH))))

XS(XS_Cwd_extLibpath)
{
    dXSARGS;
    if (items < 0 || items > 1)
	croak("Usage: Cwd::extLibpath(type = 0)");
    {
	bool	type;
	char	to[1024];
	U32	rc;
	char *	RETVAL;

	if (items < 1)
	    type = 0;
	else {
	    type = (int)SvIV(ST(0));
	}

	RETVAL = extLibpath(type);
	ST(0) = sv_newmortal();
	sv_setpv((SV*)ST(0), RETVAL);
    }
    XSRETURN(1);
}

XS(XS_Cwd_extLibpath_set)
{
    dXSARGS;
    if (items < 1 || items > 2)
	croak("Usage: Cwd::extLibpath_set(s, type = 0)");
    {
	char *	s = (char *)SvPV(ST(0),na);
	bool	type;
	U32	rc;
	bool	RETVAL;

	if (items < 2)
	    type = 0;
	else {
	    type = (int)SvIV(ST(1));
	}

	RETVAL = extLibpath_set(s, type);
	ST(0) = boolSV(RETVAL);
	if (SvREFCNT(ST(0))) sv_2mortal(ST(0));
    }
    XSRETURN(1);
}

int
Xs_OS2_init()
{
    char *file = __FILE__;
    {
	GV *gv;

	if (_emx_env & 0x200) {	/* OS/2 */
            newXS("File::Copy::syscopy", XS_File__Copy_syscopy, file);
            newXS("Cwd::extLibpath", XS_Cwd_extLibpath, file);
            newXS("Cwd::extLibpath_set", XS_Cwd_extLibpath_set, file);
	}
        newXS("DynaLoader::mod2fname", XS_DynaLoader_mod2fname, file);
        newXS("Cwd::current_drive", XS_Cwd_current_drive, file);
        newXS("Cwd::sys_chdir", XS_Cwd_sys_chdir, file);
        newXS("Cwd::change_drive", XS_Cwd_change_drive, file);
        newXS("Cwd::sys_is_absolute", XS_Cwd_sys_is_absolute, file);
        newXS("Cwd::sys_is_rooted", XS_Cwd_sys_is_rooted, file);
        newXS("Cwd::sys_is_relative", XS_Cwd_sys_is_relative, file);
        newXS("Cwd::sys_cwd", XS_Cwd_sys_cwd, file);
        newXS("Cwd::sys_abspath", XS_Cwd_sys_abspath, file);
	gv = gv_fetchpv("OS2::is_aout", TRUE, SVt_PV);
	GvMULTI_on(gv);
#ifdef PERL_IS_AOUT
	sv_setiv(GvSV(gv), 1);
#endif 
    }
}

OS2_Perl_data_t OS2_Perl_data;

void
Perl_OS2_init(char **env)
{
    char *shell;

    settmppath();
    OS2_Perl_data.xs_init = &Xs_OS2_init;
    if (environ == NULL) {
	environ = env;
    }
    if ( (shell = getenv("PERL_SH_DRIVE")) ) {
	New(1304, sh_path, strlen(SH_PATH) + 1, char);
	strcpy(sh_path, SH_PATH);
	sh_path[0] = shell[0];
    } else if ( (shell = getenv("PERL_SH_DIR")) ) {
	int l = strlen(shell), i;
	if (shell[l-1] == '/' || shell[l-1] == '\\') {
	    l--;
	}
	New(1304, sh_path, l + 8, char);
	strncpy(sh_path, shell, l);
	strcpy(sh_path + l, "/sh.exe");
	for (i = 0; i < l; i++) {
	    if (sh_path[i] == '\\') sh_path[i] = '/';
	}
    }
}

#undef tmpnam
#undef tmpfile

char *
my_tmpnam (char *str)
{
    char *p = getenv("TMP"), *tpath;
    int len;

    if (!p) p = getenv("TEMP");
    tpath = tempnam(p, "pltmp");
    if (str && tpath) {
	strcpy(str, tpath);
	return str;
    }
    return tpath;
}

FILE *
my_tmpfile ()
{
    struct stat s;

    stat(".", &s);
    if (s.st_mode & S_IWOTH) {
	return tmpfile();
    }
    return fopen(my_tmpnam(NULL), "w+b"); /* Race condition, but
					     grants TMP. */
}

#undef flock

/* This code was contributed by Rocco Caputo. */
int 
my_flock(int handle, int op)
{
  FILELOCK      rNull, rFull;
  ULONG         timeout, handle_type, flag_word;
  APIRET        rc;
  int           blocking, shared;
  static int	use_my = -1;

  if (use_my == -1) {
    char *s = getenv("USE_PERL_FLOCK");
    if (s)
	use_my = atoi(s);
    else 
	use_my = 1;
  }
  if (!(_emx_env & 0x200) || !use_my) 
    return flock(handle, op);	/* Delegate to EMX. */
  
                                        // is this a file?
  if ((DosQueryHType(handle, &handle_type, &flag_word) != 0) ||
      (handle_type & 0xFF))
  {
    errno = EBADF;
    return -1;
  }
                                        // set lock/unlock ranges
  rNull.lOffset = rNull.lRange = rFull.lOffset = 0;
  rFull.lRange = 0x7FFFFFFF;
                                        // set timeout for blocking
  timeout = ((blocking = !(op & LOCK_NB))) ? 100 : 1;
                                        // shared or exclusive?
  shared = (op & LOCK_SH) ? 1 : 0;
                                        // do not block the unlock
  if (op & (LOCK_UN | LOCK_SH | LOCK_EX)) {
    rc = DosSetFileLocks(handle, &rFull, &rNull, timeout, shared);
    switch (rc) {
      case 0:
        errno = 0;
        return 0;
      case ERROR_INVALID_HANDLE:
        errno = EBADF;
        return -1;
      case ERROR_SHARING_BUFFER_EXCEEDED:
        errno = ENOLCK;
        return -1;
      case ERROR_LOCK_VIOLATION:
        break;                          // not an error
      case ERROR_INVALID_PARAMETER:
      case ERROR_ATOMIC_LOCK_NOT_SUPPORTED:
      case ERROR_READ_LOCKS_NOT_SUPPORTED:
        errno = EINVAL;
        return -1;
      case ERROR_INTERRUPT:
        errno = EINTR;
        return -1;
      default:
        errno = EINVAL;
        return -1;
    }
  }
                                        // lock may block
  if (op & (LOCK_SH | LOCK_EX)) {
                                        // for blocking operations
    for (;;) {
      rc =
        DosSetFileLocks(
                handle,
                &rNull,
                &rFull,
                timeout,
                shared
        );
      switch (rc) {
        case 0:
          errno = 0;
          return 0;
        case ERROR_INVALID_HANDLE:
          errno = EBADF;
          return -1;
        case ERROR_SHARING_BUFFER_EXCEEDED:
          errno = ENOLCK;
          return -1;
        case ERROR_LOCK_VIOLATION:
          if (!blocking) {
            errno = EWOULDBLOCK;
            return -1;
          }
          break;
        case ERROR_INVALID_PARAMETER:
        case ERROR_ATOMIC_LOCK_NOT_SUPPORTED:
        case ERROR_READ_LOCKS_NOT_SUPPORTED:
          errno = EINVAL;
          return -1;
        case ERROR_INTERRUPT:
          errno = EINTR;
          return -1;
        default:
          errno = EINVAL;
          return -1;
      }
                                        // give away timeslice
      DosSleep(1);
    }
  }

  errno = 0;
  return 0;
}
