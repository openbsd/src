/*	$NetBSD: pcnfsd_print.c,v 1.3 1995/08/14 19:45:18 gwr Exp $	*/

/* RE_SID: @(%)/usr/dosnfs/shades_SCCS/unix/pcnfsd/v2/src/SCCS/s.pcnfsd_print.c 1.7 92/01/24 19:58:58 SMI */
/*
**=====================================================================
** Copyright (c) 1986,1987,1988,1989,1990,1991 by Sun Microsystems, Inc.
**	@(#)pcnfsd_print.c	1.7	1/24/92
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
#include <stdlib.h>
#include <stdio.h>
#include <pwd.h>
#include <sys/file.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>

#ifndef SYSV
#include <sys/wait.h>
#endif

#ifdef ISC_2_0
#include <sys/fcntl.h>
#endif

#ifdef SHADOW_SUPPORT
#include <shadow.h>
#endif

#include "paths.h"

/*
**---------------------------------------------------------------------
** Other #define's 
**---------------------------------------------------------------------
*/
#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif

/*
** The following defintions give the maximum time allowed for
** an external command to run (in seconds)
*/
#define MAXTIME_FOR_PRINT	10
#define MAXTIME_FOR_QUEUE	10
#define MAXTIME_FOR_CANCEL	10
#define MAXTIME_FOR_STATUS	10

#define QMAX 50

/*
** The following is derived from ucb/lpd/displayq.c
*/
#define SIZECOL 62
#define FILECOL 24

extern void     scramble();
extern void     run_ps630();
extern char    *crypt();
extern FILE    *su_popen();
extern int      su_pclose();
int             build_pr_list();
char 	       *map_printer_name();
char	       *expand_alias();
void           *grab();
void            free_pr_list_item();
void            free_pr_queue_item();
pr_list		list_virtual_printers();

/*
**---------------------------------------------------------------------
**                       Misc. variable definitions
**---------------------------------------------------------------------
*/

extern int      errno;
extern int	interrupted;	/* in pcnfsd_misc.c */
struct stat     statbuf;
char            pathname[MAXPATHLEN];
char            new_pathname[MAXPATHLEN];
char            sp_name[MAXPATHLEN] = SPOOLDIR;
char            tempstr[256];
char            delims[] = " \t\r\n:()";

pr_list         printers = NULL;
pr_queue        queue = NULL;

/*
**=====================================================================
**                      C O D E   S E C T I O N                       *
**=====================================================================
*/

/*
 * This is the latest word on the security check. The following
 * routine "suspicious()" returns non-zero if the character string
 * passed to it contains any shell metacharacters.
 * Callers will typically code
 *
 *	if(suspicious(some_parameter)) reject();
 */

int suspicious (s)
char *s;
{
	if(strpbrk(s, ";|&<>`'#!?*()[]^/${}\n\r\"\\:") != NULL)
		return 1;
	return 0;
}


int
valid_pr(pr)
char *pr;
{
char *p;
pr_list curr;
	if(printers == NULL)
		build_pr_list();

	if(printers == NULL)
		return(1); /* can't tell - assume it's good */

	p = map_printer_name(pr);
	if (p == NULL)
		return(1);	/* must be ok is maps to NULL! */
	curr = printers;
	while(curr) {
		if(!strcmp(p, curr->pn))
			return(1);
		curr = curr->pr_next;
	}
		
	return(0);
}

/*
 * get pathname of current directory and return to client
 *
 * Note: This runs as root on behalf of a client request.
 * As described in CERT advisory CA-96.08, be careful about
 * doing a chmod on something that could be a symlink...
 */
pirstat pr_init(sys, pr, sp)
char *sys;
char *pr;
char**sp;
{
	int    dir_mode = 0777;
	int rc;
	mode_t oldmask;

	*sp = &pathname[0];
	pathname[0] = '\0';

	if(suspicious(sys) || suspicious(pr))
		return(PI_RES_FAIL);

	/*
	 * Create the client spool directory if needed.
	 * Just do the mkdir call and ignore EEXIST.
	 * Mode of client directory should be 777.
	 */
	(void)snprintf(pathname, sizeof pathname, "%s/%s",sp_name, sys);
	oldmask = umask(0);
	rc = mkdir(pathname, dir_mode);	/* DON'T ignore this return code */
	umask(oldmask);

	if((rc < 0 && errno != EEXIST) ||
	   (stat(pathname, &statbuf) != 0) ||
	   !(statbuf.st_mode & S_IFDIR)) {
		(void)snprintf(tempstr, sizeof tempstr,
		    "rpc.pcnfsd: unable to set up spool directory %s\n",
		 	  pathname);
		msg_out(tempstr);
	    pathname[0] = '\0';	/* null to tell client bad vibes */
	    return(PI_RES_FAIL);
	}

	/* OK, we have a spool directory. */
 	if (!valid_pr(pr)) {
	    pathname[0] = '\0';	/* null to tell client bad vibes */
	    return(PI_RES_NO_SUCH_PRINTER);
	} 
	return(PI_RES_OK);

}


psrstat pr_start2(system, pr, user, fname, opts, id)
char *system;
char *pr;
char *user;
char *fname;
char *opts;
char **id;
{
char            snum[20];
static char     req_id[256];
char            cmdbuf[256];
char            resbuf[256];
FILE *fd;
int i;
char *xcmd;
char *cp;
int failed = 0;

#ifdef HACK_FOR_ROTATED_TRANSCRIPT
char            scratch[512];
#endif


	if(suspicious(system) || 
		suspicious(pr) ||
		suspicious(user) ||
		suspicious(fname))
		return(PS_RES_FAIL);

	(void)snprintf(pathname, sizeof pathname, "%s/%s/%s",sp_name,
	                         system,
	                         fname);	

	*id = &req_id[0];
	req_id[0] = '\0';

	if (stat(pathname, &statbuf)) 
           {
	   /*
           **-----------------------------------------------------------------
	   ** We can't stat the file. Let's try appending '.spl' and
	   ** see if it's already in progress.
           **-----------------------------------------------------------------
	   */

	   (void)strcat(pathname, ".spl");
	   if (stat(pathname, &statbuf)) 
	      {
	      /*
              **----------------------------------------------------------------
	      ** It really doesn't exist.
              **----------------------------------------------------------------
	      */


	      return(PS_RES_NO_FILE);
	      }
	      /*
              **-------------------------------------------------------------
	      ** It is already on the way.
              **-------------------------------------------------------------
	      */


		return(PS_RES_ALREADY);
	     }

	if (statbuf.st_size == 0) 
	   {
	   /*
           **-------------------------------------------------------------
	   ** Null file - don't print it, just kill it.
           **-------------------------------------------------------------
	   */
	   (void)unlink(pathname);

	    return(PS_RES_NULL);
	    }
	 /*
         **-------------------------------------------------------------
	 ** The file is real, has some data, and is not already going out.
	 ** We rename it by appending '.spl' and exec "lpr" to do the
	 ** actual work.
         **-------------------------------------------------------------
	 */
	if (snprintf(new_pathname, sizeof new_pathname, "%s.spl",
	    pathname) >= sizeof new_pathname) {
		snprintf(tempstr, sizeof tempstr,
		    "rpc.pcnfsd: spool file rename (%s->%s) failed.\n",
		    pathname, new_pathname);
		msg_out(tempstr);
		return(PS_RES_FAIL);
	}

	/*
        **-------------------------------------------------------------
	** See if the new filename exists so as not to overwrite it.
        **-------------------------------------------------------------
	*/


	if (!stat(new_pathname, &statbuf)) {
		if (snprintf(new_pathname, sizeof new_pathname, "%s%d.spl",
		    rand(), pathname) >= sizeof new_pathname) {
			snprintf(tempstr, sizeof tempstr,
			    "rpc.pcnfsd: spool file rename (%s->%s) failed.\n",
			    pathname, new_pathname);
			msg_out(tempstr);
			return(PS_RES_FAIL);
		}
	}
	if (rename(pathname, new_pathname)) 
	   {
	   /*
           **---------------------------------------------------------------
	   ** Should never happen.
           **---------------------------------------------------------------
           */
	   (void)snprintf(tempstr, sizeof tempstr,
		"rpc.pcnfsd: spool file rename (%s->%s) failed.\n",
			pathname, new_pathname);
                msg_out(tempstr);
		return(PS_RES_FAIL);
	    }

		if (*opts == 'd') 
	           {
		   /*
                   **------------------------------------------------------
		   ** This is a Diablo print stream. Apply the ps630
		   ** filter with the appropriate arguments.
                   **------------------------------------------------------
		   */
		   (void)snprintf(tempstr, sizeof tempstr,
			"rpc.pcnfsd: ps630 filter disabled for %s\n", pathname);
			msg_out(tempstr);
			return(PS_RES_FAIL);
		   }
		/*
		** Try to match to an aliased printer
		*/
		xcmd = expand_alias(pr, new_pathname, user, system);
		if(!xcmd) {
#ifdef	SVR4
	        /*
			 * Use the copy option so we can remove the orignal
			 * spooled nfs file from the spool directory.
			 */
			snprintf(cmdbuf, sizeof cmdbuf, "/usr/bin/lp -c -d%s %s",
				pr, new_pathname);
#else	/* SVR4 */
			/* BSD way: lpr */
			snprintf(cmdbuf, sizeof cmdbuf, "%s/lpr '-P%s' '%s'",
				LPRDIR, pr, new_pathname);
#endif	/* SVR4 */
			xcmd = cmdbuf;
		}
		if ((fd = su_popen(user, xcmd, MAXTIME_FOR_PRINT)) == NULL) {
			msg_out("rpc.pcnfsd: su_popen failed");
			return(PS_RES_FAIL);
		}
		req_id[0] = '\0';	/* asume failure */
		while(fgets(resbuf, 255, fd) != NULL) {
			i = strlen(resbuf);
			if(i)
				resbuf[i-1] = '\0'; /* trim NL */
			if(!strncmp(resbuf, "request id is ", 14))
				/* New - just the first word is needed */
				strcpy(req_id, strtok(&resbuf[14], delims));
			else if (strembedded("disabled", resbuf))
				failed = 1;
		}
		if(su_pclose(fd) == 255)
			msg_out("rpc.pcnfsd: su_pclose alert");
		(void)unlink(new_pathname);
		return((failed | interrupted)? PS_RES_FAIL : PS_RES_OK);
}


/*
 * build_pr_list: determine which printers are valid.
 * on SVR4 use "lpstat -v"
 * on BSD use "lpc status"
 */

#ifdef	SVR4
/*
 * In SVR4 the command to determine which printers are
 * valid is lpstat -v. The output is something like this:
 *
 * device for lp: /dev/lp0
 * system for pcdslw: hinode
 * system for bletch: hinode (as printer hisname)
 *
 * On SunOS using the SysV compatibility package, the output
 * is more like:
 *
 * device for lp is /dev/lp0
 * device for pcdslw is the remote printer pcdslw on hinode
 * device for bletch is the remote printer hisname on hinode
 *
 * It is fairly simple to create logic that will handle either
 * possibility:
 */
int
build_pr_list()
{
	pr_list last = NULL;
	pr_list curr = NULL;
	char buff[256];
	FILE *p;
	char *cp;
	int saw_system;

	p = popen("lpstat -v", "r");
	if(p == NULL) {
		msg_out("rpc.pcnfsd: unable to popen() lp status");
		return(0);
	}
	
	while(fgets(buff, 255, p) != NULL) {
		cp = strtok(buff, delims);
		if(!cp)
			continue;
		if(!strcmp(cp, "device"))
			saw_system = 0;
		else if (!strcmp(cp, "system"))
			saw_system = 1;
		else
			continue;
		cp = strtok(NULL, delims);
		if(!cp || strcmp(cp, "for"))
			continue;
		cp = strtok(NULL, delims);
		if(!cp)
			continue;
		curr = (struct pr_list_item *)
			grab(sizeof (struct pr_list_item));

		curr->pn = strdup(cp);
		curr->device = NULL;
		curr->remhost = NULL;
		curr->cm = strdup("-");
		curr->pr_next = NULL;

		cp = strtok(NULL, delims);

		if(cp && !strcmp(cp, "is")) 
			cp = strtok(NULL, delims);

		if(!cp) {
			free_pr_list_item(curr);
			continue;
		}

		if(saw_system) {
			/* "system" OR "system (as printer pname)" */ 
			curr->remhost = strdup(cp);
			cp = strtok(NULL, delims);
			if(!cp) {
				/* simple format */
				curr->device = strdup(curr->pn);
			} else {
				/* "sys (as printer pname)" */
				if (strcmp(cp, "as")) {
					free_pr_list_item(curr);
					continue;
				}
				cp = strtok(NULL, delims);
				if (!cp || strcmp(cp, "printer")) {
					free_pr_list_item(curr);
					continue;
				}
				cp = strtok(NULL, delims);
				if(!cp) {
					free_pr_list_item(curr);
					continue;
				}
				curr->device = strdup(cp);
			}
		}
		else if(!strcmp(cp, "the")) {
			/* start of "the remote printer foo on bar" */
			cp = strtok(NULL, delims);
			if(!cp || strcmp(cp, "remote")) {
				free_pr_list_item(curr);
				continue;
			}
			cp = strtok(NULL, delims);
			if(!cp || strcmp(cp, "printer")) {
				free_pr_list_item(curr);
				continue;
			}
			cp = strtok(NULL, delims);
			if(!cp) {
				free_pr_list_item(curr);
				continue;
			}
			curr->device = strdup(cp);
			cp = strtok(NULL, delims);
			if(!cp || strcmp(cp, "on")) {
				free_pr_list_item(curr);
				continue;
			}
			cp = strtok(NULL, delims);
			if(!cp) {
				free_pr_list_item(curr);
				continue;
			}
			curr->remhost = strdup(cp);
		} else {
			/* the local name */
			curr->device = strdup(cp);
			curr->remhost = strdup("");
		}

		if(last == NULL)
			printers = curr;
		else
			last->pr_next = curr;
		last = curr;

	}
	(void) pclose(p);

	/*
	 ** Now add on the virtual printers, if any
	 */
	if(last == NULL)
		printers = list_virtual_printers();
	else
		last->pr_next = list_virtual_printers();

	return(1);
}

#else	/* SVR4 */

/*
 * BSD way: lpc stat
 */
int
build_pr_list()
{
	pr_list last = NULL;
	pr_list curr = NULL;
	char buff[256];
	FILE *p;
	char *cp;
	int saw_system;

	snprintf(buff, sizeof buff, "%s/lpc status", LPCDIR);
	p = popen(buff, "r");
	if(p == NULL) {
		msg_out("rpc.pcnfsd: unable to popen lpc stat");
		return(0);
	}
	
	while(fgets(buff, 255, p) != NULL) {
		if (isspace(buff[0]))
			continue;

		if ((cp = strtok(buff, delims)) == NULL)
			continue;

		curr = (struct pr_list_item *)
			grab(sizeof (struct pr_list_item));

		/* XXX - Should distinguish remote printers. */
		curr->pn = strdup(cp);
		curr->device = strdup(cp);
		curr->remhost = strdup("");
		curr->cm = strdup("-");
		curr->pr_next = NULL;

		if(last == NULL)
			printers = curr;
		else
			last->pr_next = curr;
		last = curr;

	}
	(void) fclose(p);

	/*
	 ** Now add on the virtual printers, if any
	 */
	if(last == NULL)
		printers = list_virtual_printers();
	else
		last->pr_next = list_virtual_printers();

	return(1);
}

#endif	/* SVR4 */

void *grab(n)
int n;
{
	void *p;

	p = (void *)malloc(n);
	if(p == NULL) {
		msg_out("rpc.pcnfsd: malloc failure");
		exit(1);
	}
	return(p);
}

void
free_pr_list_item(curr)
pr_list curr;
{
	if(curr->pn)
		free(curr->pn);
	if(curr->device)
		free(curr->device);
	if(curr->remhost)
		free(curr->remhost);
	if(curr->cm)
		free(curr->cm);
	if(curr->pr_next)
		free_pr_list_item(curr->pr_next); /* recurse */
	free(curr);
}

/*
 * build_pr_queue:  used to show the print queue.
 *
 * Note that the first thing we do is to discard any
 * existing queue.
 */
#ifdef SVR4

/*
** In SVR4 the command to list the print jobs for printer
** lp is "lpstat lp" (or, equivalently, "lpstat -p lp").
** The output looks like this:
** 
** lp-2                    root               939   Jul 10 21:56
** lp-5                    geoff               15   Jul 12 23:23
** lp-6                    geoff               15   Jul 12 23:23
** 
** If the first job is actually printing the first line
** is modified, as follows:
**
** lp-2                    root               939   Jul 10 21:56 on lp
** 
** I don't yet have any info on what it looks like if the printer
** is remote and we're spooling over the net. However for
** the purposes of rpc.pcnfsd we can simply say that field 1 is the
** job ID, field 2 is the submitter, and field 3 is the size.
** We can check for the presence of the string " on " in the
** first record to determine if we should count it as rank 0 or rank 1,
** but it won't hurt if we get it wrong.
**/

pirstat
build_pr_queue(pn, user, just_mine, p_qlen, p_qshown)
printername     pn;
username        user;
int            just_mine;
int            *p_qlen;
int            *p_qshown;
{
pr_queue last = NULL;
pr_queue curr = NULL;
char buff[256];
FILE *p;
char *owner;
char *job;
char *totsize;

	if(queue) {
		free_pr_queue_item(queue);
		queue = NULL;
	}
	*p_qlen = 0;
	*p_qshown = 0;

	pn = map_printer_name(pn);
	if(pn == NULL || !valid_pr(pn) || suspicious(pn))
		return(PI_RES_NO_SUCH_PRINTER);

	snprintf(buff, sizeof buff, "/usr/bin/lpstat %s", pn);
	p = su_popen(user, buff, MAXTIME_FOR_QUEUE);
	if(p == NULL) {
		msg_out("rpc.pcnfsd: unable to popen() lpstat queue query");
		return(PI_RES_FAIL);
	}
	
	while(fgets(buff, 255, p) != NULL) {
		job = strtok(buff, delims);
		if(!job)
			continue;

		owner = strtok(NULL, delims);
		if(!owner)
			continue;

		totsize = strtok(NULL, delims);
		if(!totsize)
			continue;

		*p_qlen += 1;

		if(*p_qshown > QMAX)
			continue;

		if(just_mine && mystrcasecmp(owner, user))
			continue;

		*p_qshown += 1;

		curr = (struct pr_queue_item *)
			grab(sizeof (struct pr_queue_item));

		curr->position = *p_qlen;
		curr->id = strdup(job);
		curr->size = strdup(totsize);
		curr->status = strdup("");
		curr->system = strdup("");
		curr->user = strdup(owner);
		curr->file = strdup("");
		curr->cm = strdup("-");
		curr->pr_next = NULL;

		if(last == NULL)
			queue = curr;
		else
			last->pr_next = curr;
		last = curr;

	}
	(void) su_pclose(p);
	return(PI_RES_OK);
}

#else /* SVR4 */

pirstat
build_pr_queue(pn, user, just_mine, p_qlen, p_qshown)
printername     pn;
username        user;
int            just_mine;
int            *p_qlen;
int            *p_qshown;
{
pr_queue last = NULL;
pr_queue curr = NULL;
char buff[256];
FILE *p;
char *cp;
int i;
char *rank;
char *owner;
char *job;
char *files;
char *totsize;

	if(queue) {
		free_pr_queue_item(queue);
		queue = NULL;
	}
	*p_qlen = 0;
	*p_qshown = 0;
	pn = map_printer_name(pn);
	if(pn == NULL || suspicious(pn))
		return(PI_RES_NO_SUCH_PRINTER);

	snprintf(buff, sizeof buff, "%s/lpq '-P%s'", LPRDIR, pn);

	p = su_popen(user, buff, MAXTIME_FOR_QUEUE);
	if(p == NULL) {
		msg_out("rpc.pcnfsd: unable to popen() lpq");
		return(PI_RES_FAIL);
	}
	
	while(fgets(buff, 255, p) != NULL) {
		i = strlen(buff) - 1;
		buff[i] = '\0';		/* zap trailing NL */
		if(i < SIZECOL)
			continue;
		if(!mystrncasecmp(buff, "rank", 4))
			continue;

		totsize = &buff[SIZECOL-1];
		files = &buff[FILECOL-1];
		cp = totsize;
		cp--;
		while(cp > files && isspace(*cp))
			*cp-- = '\0';

		buff[FILECOL-2] = '\0';

		cp = strtok(buff, delims);
		if(!cp)
			continue;
		rank = cp;

		cp = strtok(NULL, delims);
		if(!cp)
			continue;
		owner = cp;

		cp = strtok(NULL, delims);
		if(!cp)
			continue;
		job = cp;

		*p_qlen += 1;

		if(*p_qshown > QMAX)
			continue;

		if(just_mine && mystrcasecmp(owner, user))
			continue;

		*p_qshown += 1;

		curr = (struct pr_queue_item *)
			grab(sizeof (struct pr_queue_item));

		curr->position = atoi(rank); /* active -> 0 */
		curr->id = strdup(job);
		curr->size = strdup(totsize);
		curr->status = strdup(rank);
		curr->system = strdup("");
		curr->user = strdup(owner);
		curr->file = strdup(files);
		curr->cm = strdup("-");
		curr->pr_next = NULL;

		if(last == NULL)
			queue = curr;
		else
			last->pr_next = curr;
		last = curr;

	}
	(void) su_pclose(p);
	return(PI_RES_OK);
}

#endif /* SVR4 */

void
free_pr_queue_item(curr)
pr_queue curr;
{
	if(curr->id)
		free(curr->id);
	if(curr->size)
		free(curr->size);
	if(curr->status)
		free(curr->status);
	if(curr->system)
		free(curr->system);
	if(curr->user)
		free(curr->user);
	if(curr->file)
		free(curr->file);
	if(curr->cm)
		free(curr->cm);
	if(curr->pr_next)
		free_pr_queue_item(curr->pr_next); /* recurse */
	free(curr);
}

#ifdef SVR4

/*
** New - SVR4 printer status handling.
**
** The command we'll use for checking the status of printer "lp"
** is "lpstat -a lp -p lp". Here are some sample outputs:
**
** 
** lp accepting requests since Wed Jul 10 21:49:25 EDT 1991
** printer lp disabled since Thu Feb 21 22:52:36 EST 1991. available.
** 	new printer
** ---
** pcdslw not accepting requests since Fri Jul 12 22:30:00 EDT 1991 -
** 	unknown reason
** printer pcdslw disabled since Fri Jul 12 22:15:37 EDT 1991. available.
** 	new printer
** ---
** lp accepting requests since Wed Jul 10 21:49:25 EDT 1991
** printer lp now printing lp-2. enabled since Sat Jul 13 12:02:17 EDT 1991. available.
** ---
** lp accepting requests since Wed Jul 10 21:49:25 EDT 1991
** printer lp now printing lp-2. enabled since Sat Jul 13 12:02:17 EDT 1991. available.
** ---
** lp accepting requests since Wed Jul 10 21:49:25 EDT 1991
** printer lp disabled since Sat Jul 13 12:05:20 EDT 1991. available.
** 	unknown reason
** ---
** pcdslw not accepting requests since Fri Jul 12 22:30:00 EDT 1991 -
** 	unknown reason
** printer pcdslw is idle. enabled since Sat Jul 13 12:05:28 EDT 1991. available.
**
** Note that these are actual outputs. The format (which is totally
** different from the lpstat in SunOS) seems to break down as
** follows:
** (1) The first line has the form "printername [not] accepting requests,,,"
**    This is trivial to decode.
** (2) The second line has several forms, all beginning "printer printername":
** (2.1) "... disabled"
** (2.2) "... is idle"
** (2.3) "... now printing jobid"
** The "available" comment seems to be meaningless. The next line
** is the "reason" code which the operator can supply when issuing
** a "disable" or "reject" command.
** Note that there is no way to check the number of entries in the
** queue except to ask for the queue and count them.
*/

pirstat
get_pr_status(pn, avail, printing, qlen, needs_operator, status)
printername   pn;
bool_t       *avail;
bool_t       *printing;
int          *qlen;
bool_t       *needs_operator;
char         *status;
{
char buff[256];
char cmd[64];
FILE *p;
int n;
pirstat stat = PI_RES_NO_SUCH_PRINTER;

	/* assume the worst */
	*avail = FALSE;
	*printing = FALSE;
	*needs_operator = FALSE;
	*qlen = 0;
	*status = '\0';

	pn = map_printer_name(pn);
	if(pn == NULL || !valid_pr(pn) || suspicious(pn))
		return(PI_RES_NO_SUCH_PRINTER);
	n = strlen(pn);

	snprintf(cmd, sizeof cmd, "/usr/bin/lpstat -a %s -p %s", pn, pn);

	p = popen(cmd, "r");
	if(p == NULL) {
		msg_out("rpc.pcnfsd: unable to popen() lp status");
		return(PI_RES_FAIL);
	}
	
	stat = PI_RES_OK;

	while(fgets(buff, 255, p) != NULL) {
		if(!strncmp(buff, pn, n)) {
			if(!strstr(buff, "not accepting"))
			*avail = TRUE;
			continue;
		}
		if(!strncmp(buff, "printer ", 8)) {
			if(!strstr(buff, "disabled"))
				*printing = TRUE;
			if(strstr(buff, "printing"))
				strcpy(status, "printing");
			else if (strstr(buff, "idle"))
				strcpy(status, "idle");
			continue;
		}
		if(!strncmp(buff, "UX:", 3)) {
			stat = PI_RES_NO_SUCH_PRINTER;
		}
	}
	(void) pclose(p);
	return(stat);
}

#else /* SVR4 */

/*
 * BSD way: lpc status
 */
pirstat
get_pr_status(pn, avail, printing, qlen, needs_operator, status)
printername   pn;
bool_t       *avail;
bool_t       *printing;
int          *qlen;
bool_t       *needs_operator;
char         *status;
{
	char cmd[128];
	char buff[256];
	char buff2[256];
	char pname[64];
	FILE *p;
	char *cp;
	char *cp1;
	char *cp2;
	int n;
	pirstat stat = PI_RES_NO_SUCH_PRINTER;

	/* assume the worst */
	*avail = FALSE;
	*printing = FALSE;
	*needs_operator = FALSE;
	*qlen = 0;
	*status = '\0';

	pn = map_printer_name(pn);
	if(pn == NULL || suspicious(pn) || !valid_pr(pn))
		return(PI_RES_NO_SUCH_PRINTER);

	snprintf(pname, sizeof pname, "%s:", pn);
	n = strlen(pname);

	snprintf(cmd, sizeof cmd, "%s/lpc status '%s'", LPCDIR, pn);
	p = popen(cmd, "r");
	if(p == NULL) {
		msg_out("rpc.pcnfsd: unable to popen() lp status");
		return(PI_RES_FAIL);
	}
	
	while(fgets(buff, 255, p) != NULL) {
		if(strncmp(buff, pname, n))
			continue;
/*
** We have a match. The only failure now is PI_RES_FAIL if
** lpstat output cannot be decoded
*/
		stat = PI_RES_FAIL;
/*
** The next four lines are usually if the form
**
**     queuing is [enabled|disabled]
**     printing is [enabled|disabled]
**     [no entries | N entr[y|ies] in spool area]
**     <status message, may include the word "attention">
*/
		while(fgets(buff, 255, p) != NULL && isspace(buff[0])) {
			cp = buff;
			while(isspace(*cp))
				cp++;
			if(*cp == '\0')
				break;
			cp1 = cp;
			cp2 = buff2;
			while (*cp1 && *cp1 != '\n' &&
			    cp2 < &buff2[sizeof buff2] - 2) {
				*cp2++ = tolower(*cp1);
				cp1++;
			}
			*cp1 = '\0';
			*cp2 = '\0';
/*
** Now buff2 has a lower-cased copy and cp points at the original;
** both are null terminated without any newline
*/			
			if(!strncmp(buff2, "queuing", 7)) {
				*avail = (strstr(buff2, "enabled") != NULL);
				continue;
			}
			if(!strncmp(buff2, "printing", 8)) {
				*printing = (strstr(buff2, "enabled") != NULL);
				continue;
			}
			if(isdigit(buff2[0]) && (strstr(buff2, "entr") !=NULL)) {

				*qlen = atoi(buff2);
				continue;
			}
			if(strstr(buff2, "attention") != NULL ||
			   strstr(buff2, "error") != NULL)
				*needs_operator = TRUE;
			if(*needs_operator || strstr(buff2, "waiting") != NULL) {
				strncpy(status, cp, 127);
				status[127] = '\0';
			}
		}
		stat = PI_RES_OK;
		break;
	}
	(void) pclose(p);
	return(stat);
}

#endif /* SVR4 */

/*
 * pr_cancel: cancel a print job
 */
#ifdef SVR4

/*
** For SVR4 we have to be prepared for the following kinds of output:
** 
** # cancel lp-6
** request "lp-6" cancelled
** # cancel lp-33
** UX:cancel: WARNING: Request "lp-33" doesn't exist.
** # cancel foo-88
** UX:cancel: WARNING: Request "foo-88" doesn't exist.
** # cancel foo
** UX:cancel: WARNING: "foo" is not a request id or a printer.
**             TO FIX: Cancel requests by id or by
**                     name of printer where printing.
** # su geoff
** $ cancel lp-2
** UX:cancel: WARNING: Can't cancel request "lp-2".
**             TO FIX: You are not allowed to cancel
**                     another's request.
**
** There are probably other variations for remote printers.
** Basically, if the reply begins with the string
**          "UX:cancel: WARNING: "
** we can strip this off and look for one of the following
** (1) 'R' - should be part of "Request "xxxx" doesn't exist."
** (2) '"' - should be start of ""foo" is not a request id or..."
** (3) 'C' - should be start of "Can't cancel request..."
**
** The fly in the ointment: all of this can change if these
** messages are localized..... :-(
*/
pcrstat pr_cancel(pr, user, id)
char *pr;
char *user;
char *id;
{
char            cmdbuf[256];
char            resbuf[256];
FILE *fd;
pcrstat stat = PC_RES_NO_SUCH_JOB;

	pr = map_printer_name(pr);
	if(pr == NULL || suspicious(pr))
		return(PC_RES_NO_SUCH_PRINTER);
	if(suspicious(id))
		return(PC_RES_NO_SUCH_JOB);

	snprintf(cmdbuf, sizeof cmdbuf, "/usr/bin/cancel %s", id);
	if ((fd = su_popen(user, cmdbuf, MAXTIME_FOR_CANCEL)) == NULL) {
		msg_out("rpc.pcnfsd: su_popen failed");
		return(PC_RES_FAIL);
	}

	if(fgets(resbuf, 255, fd) == NULL) 
		stat = PC_RES_FAIL;
	else if(!strstr(resbuf, "UX:"))
		stat = PC_RES_OK;
	else if(strstr(resbuf, "doesn't exist"))
		stat = PC_RES_NO_SUCH_JOB;
	else if(strstr(resbuf, "not a request id"))
		stat = PC_RES_NO_SUCH_JOB;
	else if(strstr(resbuf, "Can't cancel request"))
		stat = PC_RES_NOT_OWNER;
	else	stat = PC_RES_FAIL;

	if(su_pclose(fd) == 255)
		msg_out("rpc.pcnfsd: su_pclose alert");
	return(stat);
}

#else /* SVR4 */

/*
 * BSD way: lprm
 */
pcrstat pr_cancel(pr, user, id)
char *pr;
char *user;
char *id;
{
	char            cmdbuf[256];
	char            resbuf[256];
	FILE *fd;
	int i;
	pcrstat stat = PC_RES_NO_SUCH_JOB;

	pr = map_printer_name(pr);
	if(pr == NULL || suspicious(pr))
		return(PC_RES_NO_SUCH_PRINTER);
	if(suspicious(id))
		return(PC_RES_NO_SUCH_JOB);

		snprintf(cmdbuf, sizeof cmdbuf, "%s/lprm '-P%s' '%s'",
		    LPRDIR, pr, id);
		if ((fd = su_popen(user, cmdbuf, MAXTIME_FOR_CANCEL)) == NULL) {
			msg_out("rpc.pcnfsd: su_popen failed");
			return(PC_RES_FAIL);
		}
		while(fgets(resbuf, 255, fd) != NULL) {
			i = strlen(resbuf);
			if(i)
				resbuf[i-1] = '\0'; /* trim NL */
			if(strstr(resbuf, "dequeued") != NULL)
				stat = PC_RES_OK;
			if(strstr(resbuf, "unknown printer") != NULL)
				stat = PC_RES_NO_SUCH_PRINTER;
			if(strstr(resbuf, "Permission denied") != NULL)
				stat = PC_RES_NOT_OWNER;
		}
		if(su_pclose(fd) == 255)
			msg_out("rpc.pcnfsd: su_pclose alert");
		return(stat);
}
#endif /* SVR4 */

/*
** New subsystem here. We allow the administrator to define
** up to NPRINTERDEFS aliases for printer names. This is done
** using the "/etc/pcnfsd.conf" file, which is read at startup.
** There are three entry points to this subsystem
**
** void add_printer_alias(char *printer, char *alias_for, char *command)
**
** This is invoked from "config_from_file()" for each
** "printer" line. "printer" is the name of a printer; note that
** it is possible to redefine an existing printer. "alias_for"
** is the name of the underlying printer, used for queue listing
** and other control functions. If it is "-", there is no
** underlying printer, or the administrative functions are
** not applicable to this printer. "command"
** is the command which should be run (via "su_popen()") if a
** job is printed on this printer. The following tokens may be
** embedded in the command, and are substituted as follows:
**
** $FILE	-	path to the file containing the print data
** $USER	-	login of user
** $HOST	-	hostname from which job originated
**
** Tokens may occur multiple times. If The command includes no
** $FILE token, the string " $FILE" is silently appended.
**
** pr_list list_virtual_printers()
**
** This is invoked from build_pr_list to generate a list of aliased
** printers, so that the client that asks for a list of valid printers
** will see these ones.
**
** char *map_printer_name(char *printer)
**
** If "printer" identifies an aliased printer, this function returns
** the "alias_for" name, or NULL if the "alias_for" was given as "-".
** Otherwise it returns its argument.
**
** char *expand_alias(char *printer, char *file, char *user, char *host)
**
** If "printer" is an aliased printer, this function returns a
** pointer to a static string in which the corresponding command
** has been expanded. Otherwise ot returns NULL.
*/
#define NPRINTERDEFS	16
int num_aliases = 0;
struct {
	char *a_printer;
	char *a_alias_for;
	char *a_command;
} alias [NPRINTERDEFS];



void
add_printer_alias(printer, alias_for, command)
char *printer;
char *alias_for;
char *command;
{
	if(num_aliases < NPRINTERDEFS) {
		alias[num_aliases].a_printer = strdup(printer);
		alias[num_aliases].a_alias_for =
			(strcmp(alias_for,  "-") ? strdup(alias_for) : NULL);
		if(strstr(command, "$FILE"))
			alias[num_aliases].a_command = strdup(command);
		else {
			alias[num_aliases].a_command = (char *)grab(strlen(command) + 8);
			strcpy(alias[num_aliases].a_command, command);
			strcat(alias[num_aliases].a_command, " $FILE");
		}
		num_aliases++;
	}
}

pr_list list_virtual_printers()
{
pr_list first = NULL;
pr_list last = NULL;
pr_list curr = NULL;
int i;


	if(num_aliases == 0)
		return(NULL);

	for (i = 0; i < num_aliases; i++) {
		curr = (struct pr_list_item *)
			grab(sizeof (struct pr_list_item));

		curr->pn = strdup(alias[i].a_printer);
		if(alias[i].a_alias_for == NULL)
			curr->device = strdup("");
		else
			curr->device = strdup(alias[i].a_alias_for);
		curr->remhost = strdup("");
		curr->cm = strdup("(alias)");
		curr->pr_next = NULL;
		if(last == NULL)
			first = curr;
		else
			last->pr_next = curr;
		last = curr;

	}
	return(first);
}


char *
map_printer_name(printer)
char *printer;
{
int i;
	for (i = 0; i < num_aliases; i++){
		if(!strcmp(printer, alias[i].a_printer))
			return(alias[i].a_alias_for);
	}
	return(printer);
}

static void
substitute(string, token, data)
char *string;
char *token;
char *data;
{
char temp[512];
char *c;

	while(c = strstr(string, token)) {
		*c = '\0';
		strcpy(temp, string);
		strcat(temp, data);
		c += strlen(token);
		strcat(temp, c);
		strcpy(string, temp);
	}
}

char *
expand_alias(printer, file, user, host)
char *printer;
char *file;
char *user;
char *host;
{
static char expansion[512];
int i;
	for (i = 0; i < num_aliases; i++){
		if(!strcmp(printer, alias[i].a_printer)) {
			strcpy(expansion, alias[i].a_command);
			substitute(expansion, "$FILE", file);
			substitute(expansion, "$USER", user);
			substitute(expansion, "$HOST", host);
			return(expansion);
		}
	}
	return(NULL);
}
