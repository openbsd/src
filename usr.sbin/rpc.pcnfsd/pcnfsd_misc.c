/*	$NetBSD: pcnfsd_misc.c,v 1.2 1995/07/25 22:20:42 gwr Exp $	*/

/* RE_SID: @(%)/usr/dosnfs/shades_SCCS/unix/pcnfsd/v2/src/SCCS/s.pcnfsd_misc.c 1.5 92/01/24 19:59:13 SMI */
/*
**=====================================================================
** Copyright (c) 1986,1987,1988,1989,1990,1991 by Sun Microsystems, Inc.
**	@(#)pcnfsd_misc.c	1.5	1/24/92
**=====================================================================
*/
#include "common.h"
/*
**=====================================================================
**             I N C L U D E   F I L E   S E C T I O N                *
**                                                                    *
** If your port requires different include files, add a suitable      *
** #define in the customization section, and make the inclusion or    *
** exclusion of the files conditional on this.                        *
**=====================================================================
*/
#include "pcnfsd.h"

#include <stdio.h>
#include <pwd.h>
#include <sys/file.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#ifdef ISC_2_0
#include <sys/fcntl.h>
#endif

#ifdef SHADOW_SUPPORT
#include <shadow.h>
#endif

#ifdef WTMP
int wtmp_enabled = 1;
#endif

#ifdef USE_GETUSERSHELL
extern char *getusershell();
#endif

/*
**---------------------------------------------------------------------
** Other #define's 
**---------------------------------------------------------------------
*/

#define	zchar		0x5b

char            tempstr[256];
extern char	sp_name[1024]; /* in pcnfsd_print.c */

/*
**=====================================================================
**                      C O D E   S E C T I O N                       *                    **=====================================================================
*/
/*
**---------------------------------------------------------------------
**                          Support procedures 
**---------------------------------------------------------------------
*/


void
scramble(s1, s2)
char           *s1;
char           *s2;
{
	while (*s1) 
	      {
	      *s2++ = (*s1 ^ zchar) & 0x7f;
	      s1++;
	      }
	*s2 = 0;
}



struct passwd  *
get_password(usrnam)
char           *usrnam;
{
struct passwd  *p;
static struct passwd localp;
char           *pswd;
char           *ushell;
int		ok = 0;


#ifdef SHADOW_SUPPORT
struct spwd    *sp;
int             shadowfile;
#endif

#ifdef SHADOW_SUPPORT
	/*
        **--------------------------------------------------------------
	** Check the existence of SHADOW.  If it is there, then we are
	** running a two-password-file system.
        **--------------------------------------------------------------
	*/
	if (access(SHADOW, 0))
	   shadowfile = 0;	/* SHADOW is not there */
	else
	   shadowfile = 1;

	setpwent();
	if (shadowfile)
	   (void) setspent();	/* Setting the shadow password
					 * file */
	if ((p = getpwnam(usrnam)) == (struct passwd *)NULL ||
	   (shadowfile && (sp = getspnam(usrnam)) == (struct spwd *)NULL))
	return ((struct passwd *)NULL);

	if (shadowfile) 
           {
	   pswd = sp->sp_pwdp;
	   (void) endspent();
	   } 
        else
	   pswd = p->pw_passwd;

#else
	p = getpwnam(usrnam);
	if (p == (struct passwd *)NULL)
		return ((struct passwd *)NULL);
	pswd = p->pw_passwd;
#endif

#ifdef ISC_2_0
	/* 
        **-----------------------------------------------------------
	** We may have an 'x' in which case look in /etc/shadow ..
        **-----------------------------------------------------------
        */
	if (((strlen(pswd)) == 1) && pswd[0] == 'x') 
	   {
	   struct spwd    *shadow = getspnam(usrnam);

	   if (!shadow)
	      return ((struct passwd *)NULL);
	   pswd = shadow->sp_pwdp;
	   }
#endif
	localp = *p;
	localp.pw_passwd = pswd;
#ifdef USE_GETUSERSHELL

	setusershell();
	while(ushell = getusershell()){
		if(!strcmp(ushell, localp.pw_shell)) {
			ok = 1;
			break;
		}
	}
	endusershell();
	if(!ok)
		return ((struct passwd *)NULL);
#else
/*
 * the best we can do is to ensure that the shell ends in "sh"
 */
	ushell = localp.pw_shell;
	if(strlen(ushell) < 2)
		return ((struct passwd *)NULL);
	ushell += strlen(ushell) - 2;
	if(strcmp(ushell, "sh"))
		return ((struct passwd *)NULL);

#endif
	return (&localp);
}


/*
**---------------------------------------------------------------------
**                      Print support procedures 
**---------------------------------------------------------------------
*/

char           *
mapfont(f, i, b)
	char            f;
	char            i;
	char            b;
{
	static char     fontname[64];

	fontname[0] = 0;	/* clear it out */

	switch (f) {
	case 'c':
		(void)strcpy(fontname, "Courier");
		break;
	case 'h':
		(void)strcpy(fontname, "Helvetica");
		break;
	case 't':
		(void)strcpy(fontname, "Times");
		break;
	default:
		(void)strcpy(fontname, "Times-Roman");
		goto finis ;
	}
	if (i != 'o' && b != 'b') {	/* no bold or oblique */
		if (f == 't')	/* special case Times */
			(void)strcat(fontname, "-Roman");
		goto finis;
	}
	(void)strcat(fontname, "-");
	if (b == 'b')
		(void)strcat(fontname, "Bold");
	if (i == 'o')		/* o-blique */
		(void)strcat(fontname, f == 't' ? "Italic" : "Oblique");

finis:	return (&fontname[0]);
}

/*
 * run_ps630 performs the Diablo 630 emulation filtering process. ps630
 * was broken in certain Sun releases: it would not accept point size or
 * font changes. If your version is fixed, undefine the symbol
 * PS630_IS_BROKEN and rebuild pc-nfsd.
 */
/* #define PS630_IS_BROKEN 1 */

void
run_ps630(f, opts)
	char           *f;
	char           *opts;
{
	char            temp_file[256];
	char            commbuf[256];
	int             i;

	(void)strcpy(temp_file, f);
	(void)strcat(temp_file, "X");	/* intermediate file name */

#ifndef PS630_IS_BROKEN
	(void)sprintf(commbuf, "ps630 -s %c%c -p %s -f ",
		opts[2], opts[3], temp_file);
	(void)strcat(commbuf, mapfont(opts[4], opts[5], opts[6]));
	(void)strcat(commbuf, " -F ");
	(void)strcat(commbuf, mapfont(opts[7], opts[8], opts[9]));
	(void)strcat(commbuf, "  ");
	(void)strcat(commbuf, f);
#else	/* PS630_IS_BROKEN */
	/*
	 * The pitch and font features of ps630 appear to be broken at
	 * this time.
	 */
	(void)sprintf(commbuf, "ps630 -p %s %s", temp_file, f);
#endif	/* PS630_IS_BROKEN */


	if (i = system(commbuf)) {
		/*
		 * Under (un)certain conditions, ps630 may return -1 even
		 * if it worked. Hence the commenting out of this error
		 * report.
		 */
		 /* (void)fprintf(stderr, "\n\nrun_ps630 rc = %d\n", i) */ ;
		/* exit(1); */
	}
	if (rename(temp_file, f)) {
		perror("run_ps630: rename");
		exit(1);
	}
	return;
}




/*
**---------------------------------------------------------------------
**                      WTMP update support 
**---------------------------------------------------------------------
*/


#ifdef WTMP

#include <utmp.h>

#ifndef	_PATH_WTMP
#define _PATH_WTMP "/usr/adm/wtmp"
#endif

void
wlogin(name, req)
	char *name;
	struct svc_req *req;
{
	extern char *inet_ntoa();
	struct sockaddr_in *who;
	struct hostent *hp;
	char *host;
	struct utmp ut;
	int fd;

	if(!wtmp_enabled)
		return;

	/* Get network address of client. */
	who = &req->rq_xprt->xp_raddr;

	/* Get name of connected client */
	hp = gethostbyaddr((char *)&who->sin_addr,
			   sizeof (struct in_addr),
			   who->sin_family);

	if (hp && (strlen(hp->h_name) <= sizeof(ut.ut_host))) {
		host = hp->h_name;
	} else {
		host = inet_ntoa(who->sin_addr);
	}

	(void) strcpy(ut.ut_line, "PC-NFS");
	(void) strncpy(ut.ut_name,name,sizeof ut.ut_name);
	(void) strncpy(ut.ut_host, host, sizeof ut.ut_host);
	ut.ut_time = time( (time_t *) 0);

	if ((fd = open(_PATH_WTMP, O_WRONLY|O_APPEND, 0)) >= 0) {
		(void)write(fd, (char *)&ut, sizeof(struct utmp));
		(void)close(fd);
	}
}
#endif WTMP

/*
**---------------------------------------------------------------------
**                      Run-process-as-user procedures 
**---------------------------------------------------------------------
*/


#define	READER_FD	0
#define	WRITER_FD	1

static int      child_pid;

static char     cached_user[64] = "";
static uid_t    cached_uid;
static gid_t    cached_gid;

static	struct sigaction old_action;
static	struct sigaction new_action;
static	struct itimerval timer;

int interrupted = 0;
static	FILE *pipe_handle;

static	void myhandler()
{
 interrupted = 1;
 fclose(pipe_handle);
 kill(child_pid, SIGKILL);
 msg_out("rpc.pcnfsd: su_popen timeout - killed child process");
}

void start_watchdog(n)
int n;
{
	/*
	 * Setup SIGALRM handler, force interrupt of ongoing syscall
	 */

	new_action.sa_handler = myhandler;
	sigemptyset(&(new_action.sa_mask));
	new_action.sa_flags = 0;
#ifdef SA_INTERRUPT
	new_action.sa_flags |= SA_INTERRUPT;
#endif
	sigaction(SIGALRM, &new_action, &old_action);

	/*
	 * Set interval timer for n seconds
	 */
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 0;
	timer.it_value.tv_sec = n;
	timer.it_value.tv_usec = 0;
	setitimer(ITIMER_REAL, &timer, NULL);
	interrupted = 0;

}

void stop_watchdog()
{
	/*
	 * Cancel timer
	 */

	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 0;
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = 0;
	setitimer(ITIMER_REAL, &timer, NULL);

	/*
 	 * restore old signal handling
	 */
	sigaction(SIGALRM, &old_action, NULL);
}



FILE           *
su_popen(user, cmd, maxtime)
	char           *user;
	char           *cmd;
	int		maxtime;
{
	int             p[2];
	int             parent_fd, child_fd, pid;
	struct passwd *pw;

	if (strcmp(cached_user, user)) {
		pw = getpwnam(user);
		if (!pw)
			pw = getpwnam("nobody");
		if (pw) {
			cached_uid = pw->pw_uid;
			cached_gid = pw->pw_gid;
			strcpy(cached_user, user);
		} else {
			cached_uid = (uid_t) (-2);
			cached_gid = (gid_t) (-2);
			cached_user[0] = '\0';
		}
	}
	if (pipe(p) < 0) {
		msg_out("rpc.pcnfsd: unable to create pipe in su_popen");
		return (NULL);
	}
	parent_fd = p[READER_FD];
	child_fd = p[WRITER_FD];
	if ((pid = fork()) == 0) {
		int             i;

		for (i = 0; i < 10; i++)
			if (i != child_fd)
				(void) close(i);
		if (child_fd != 1) {
			(void) dup2(child_fd, 1);
			(void) close(child_fd);
		}
		dup2(1, 2);	/* let's get stderr as well */

		(void) setgid(cached_gid);
		(void) setuid(cached_uid);

		(void) execl("/bin/sh", "sh", "-c", cmd, (char *) NULL);
		_exit(255);
	}
	if (pid == -1) {
		msg_out("rpc.pcnfsd: fork failed");
		close(parent_fd);
		close(child_fd);
		return (NULL);
	}
	child_pid = pid;
	close(child_fd);
	start_watchdog(maxtime);
	pipe_handle = fdopen(parent_fd, "r");
	return (pipe_handle);
}

int
su_pclose(ptr)
	FILE           *ptr;
{
	int             pid, status;

	stop_watchdog();

	fclose(ptr);
	if (child_pid == -1)
		return (-1);
	while ((pid = wait(&status)) != child_pid && pid != -1);
	return (pid == -1 ? -1 : status);
}


/*
** The following routine reads a file "/etc/pcnfsd.conf" if present,
** and uses it to replace certain builtin elements, like the
** name of the print spool directory. The configuration file
** Is the usual kind: Comments begin with '#', blank lines are ignored,
** and valid lines are of the form
**
**	<keyword><whitespace><value>
**
** The following keywords are recognized:
**
**	spooldir
**	printer name alias-for command
**	wtmp yes|no
*/
void
config_from_file()
{
FILE *fd;
char buff[1024];
char *cp;
char *kw;
char *val;
char *arg1;
char *arg2;

	if((fd = fopen("/etc/pcnfsd.conf", "r")) == NULL)
		return;
	while(fgets(buff, 1024, fd)) {
		cp = strchr(buff, '\n');
		*cp = '\0';
		cp = strchr(buff, '#');
		if(cp)
			*cp = '\0';
		kw = strtok(buff, " \t");
		if(kw == NULL)
			continue;
		val = strtok(NULL, " \t");
		if(val == NULL)
			continue;
		if(!mystrcasecmp(kw, "spooldir")) {
			strcpy(sp_name, val);
			continue;
		}
#ifdef WTMP
		if(!mystrcasecmp(kw, "wtmp")) {
			/* assume default is YES, just look for negatives */
			if(!mystrcasecmp(val, "no") ||
			   !mystrcasecmp(val, "off") ||
			   !mystrcasecmp(val, "disable") ||
			   !strcmp(val, "0"))
				wtmp_enabled = 0;;
			continue;
		}
#endif	
		if(!mystrcasecmp(kw, "printer")) {
			arg1 = strtok(NULL, " \t");
			arg2 = strtok(NULL, "");
			(void)add_printer_alias(val, arg1, arg2);
			continue;
		}
/*
** Add new cases here
*/
	}
	fclose(fd);
}


/*
** The following are replacements for the SunOS library
** routines strcasecmp and strncasecmp, which SVR4 doesn't
** include.
*/

mystrcasecmp(s1, s2)
	char *s1, *s2;
{

	while (toupper(*s1) == toupper(*s2++))
		if (*s1++ == '\0')
			return(0);
	return(toupper(*s1) - toupper(*--s2));
}

mystrncasecmp(s1, s2, n)
	char *s1, *s2;
	int n;
{

	while (--n >= 0 && toupper(*s1) == toupper(*s2++))
		if (*s1++ == '\0')
			return(0);
	return(n < 0 ? 0 : toupper(*s1) - toupper(*--s2));
}


/*
** strembedded - returns true if s1 is embedded (in any case) in s2
*/

int strembedded(s1, s2)
char *s1;
char *s2;
{
	while(*s2) {
		if(!mystrcasecmp(s1, s2))
			return 1;
		s2++;
	}
	return 0;
}
