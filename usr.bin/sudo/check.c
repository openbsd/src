/*
 * Copyright (c) 1993-1996,1998-2003 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * 4. Products derived from this software may not be called "Sudo" nor
 *    may "Sudo" appear in their names without specific prior written
 *    permission from the author.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif /* STDC_HEADERS */
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif /* HAVE_STRING_H */
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#ifdef HAVE_ERR_H
# include <err.h>
#else
# include "emul/err.h"
#endif /* HAVE_ERR_H */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>

#include "sudo.h"

#ifndef lint
static const char rcsid[] = "$Sudo: check.c,v 1.212 2003/04/02 18:25:19 millert Exp $";
#endif /* lint */

/* Status codes for timestamp_status() */
#define TS_CURRENT		0
#define TS_OLD			1
#define TS_MISSING		2
#define TS_NOFILE		3
#define TS_ERROR		4

static void  build_timestamp	__P((char **, char **));
static int   timestamp_status	__P((char *, char *, char *, int));
static char *expand_prompt	__P((char *, char *, char *));
static void  lecture		__P((void));
static void  update_timestamp	__P((char *, char *));

/*
 * This function only returns if the user can successfully
 * verify who he/she is.  
 */
void
check_user()
{
    char *timestampdir = NULL;
    char *timestampfile = NULL;
    char *prompt;
    int status;

    if (user_uid == 0 || user_is_exempt())
	return;

    build_timestamp(&timestampdir, &timestampfile);
    status = timestamp_status(timestampdir, timestampfile, user_name, TRUE);
    if (status != TS_CURRENT) {
	if (status == TS_MISSING || status == TS_ERROR)
	    lecture();		/* first time through they get a lecture */

	/* Expand any escapes in the prompt. */
	prompt = expand_prompt(user_prompt ? user_prompt : def_str(I_PASSPROMPT),
	    user_name, user_shost);

	verify_user(auth_pw, prompt);
    }
    if (status != TS_ERROR)
	update_timestamp(timestampdir, timestampfile);
    free(timestampdir);
    if (timestampfile)
	free(timestampfile);
}

/*
 * Standard sudo lecture.
 * TODO: allow the user to specify a file name instead.
 */
static void
lecture()
{

    if (def_flag(I_LECTURE)) {
	(void) fputs("\n\
We trust you have received the usual lecture from the local System\n\
Administrator. It usually boils down to these two things:\n\
\n\
	#1) Respect the privacy of others.\n\
	#2) Think before you type.\n\n",
	stderr);
    }
}

/*
 * Update the time on the timestamp file/dir or create it if necessary.
 */
static void
update_timestamp(timestampdir, timestampfile)
    char *timestampdir;
    char *timestampfile;
{

    if (timestamp_uid != 0)
	set_perms(PERM_TIMESTAMP);
    if (touch(timestampfile ? timestampfile : timestampdir, time(NULL)) == -1) {
	if (timestampfile) {
	    int fd = open(timestampfile, O_WRONLY|O_CREAT|O_TRUNC, 0600);

	    if (fd == -1)
		log_error(NO_EXIT|USE_ERRNO, "Can't open %s", timestampfile);
	    else
		close(fd);
	} else {
	    if (mkdir(timestampdir, 0700) == -1)
		log_error(NO_EXIT|USE_ERRNO, "Can't mkdir %s", timestampdir);
	}
    }
    if (timestamp_uid != 0)
	set_perms(PERM_ROOT);
}

/*
 * Expand %h and %u escapes in the prompt and pass back the dynamically
 * allocated result.  Returns the same string if there are no escapes.
 */
static char *
expand_prompt(old_prompt, user, host)
    char *old_prompt;
    char *user;
    char *host;
{
    size_t len, n;
    int subst;
    char *p, *np, *new_prompt, *endp;

    /* How much space do we need to malloc for the prompt? */
    subst = 0;
    for (p = old_prompt, len = strlen(old_prompt); *p; p++) {
	if (p[0] =='%') {
	    switch (p[1]) {
		case 'h':
		    p++;
		    len += strlen(user_shost) - 2;
		    subst = 1;
		    break;
		case 'H':
		    p++;
		    len += strlen(user_host) - 2;
		    subst = 1;
		    break;
		case 'u':
		    p++;
		    len += strlen(user_name) - 2;
		    subst = 1;
		    break;
		case 'U':
		    p++;
		    len += strlen(*user_runas) - 2;
		    subst = 1;
		    break;
		case '%':
		    p++;
		    len--;
		    subst = 1;
		    break;
		default:
		    break;
	    }
	}
    }

    if (subst) {
	new_prompt = (char *) emalloc(++len);
	endp = new_prompt + len;
	for (p = old_prompt, np = new_prompt; *p; p++) {
	    if (p[0] =='%') {
		switch (p[1]) {
		    case 'h':
			p++;
			n = strlcpy(np, user_shost, np - endp);
			if (n >= np - endp)
			    goto oflow;
			np += n;
			continue;
		    case 'H':
			p++;
			n = strlcpy(np, user_host, np - endp);
			if (n >= np - endp)
			    goto oflow;
			np += n;
			continue;
		    case 'u':
			p++;
			n = strlcpy(np, user_name, np - endp);
			if (n >= np - endp)
			    goto oflow;
			np += n;
			continue;
		    case 'U':
			p++;
			n = strlcpy(np,  *user_runas, np - endp);
			if (n >= np - endp)
			    goto oflow;
			np += n;
			continue;
		    case '%':
			/* convert %% -> % */
			p++;
			break;
		    default:
			/* no conversion */
			break;
		}
	    }
	    *np++ = *p;
	    if (np >= endp)
		goto oflow;
	}
	*np = '\0';
    } else
	new_prompt = old_prompt;

    return(new_prompt);

oflow:
    /* We pre-allocate enough space, so this should never happen. */
    errx(1, "internal error, expand_prompt() overflow");
}

/*
 * Checks if the user is exempt from supplying a password.
 */
int
user_is_exempt()
{
    struct group *grp;
    char **gr_mem;

    if (!def_str(I_EXEMPT_GROUP))
	return(FALSE);

    if (!(grp = getgrnam(def_str(I_EXEMPT_GROUP))))
	return(FALSE);

    if (user_gid == grp->gr_gid)
	return(TRUE);

    for (gr_mem = grp->gr_mem; *gr_mem; gr_mem++) {
	if (strcmp(user_name, *gr_mem) == 0)
	    return(TRUE);
    }

    return(FALSE);
}

/*
 * Fills in timestampdir as well as timestampfile if using tty tickets.
 */
static void
build_timestamp(timestampdir, timestampfile)
    char **timestampdir;
    char **timestampfile;
{
    char *dirparent;
    int len;

    dirparent = def_str(I_TIMESTAMPDIR);
    len = easprintf(timestampdir, "%s/%s", dirparent, user_name);
    if (len >= MAXPATHLEN)
	log_error(0, "timestamp path too long: %s", timestampdir);

    /*
     * Timestamp file may be a file in the directory or NUL to use
     * the directory as the timestamp.
     */
    if (def_flag(I_TTY_TICKETS)) {
	char *p;

	if ((p = strrchr(user_tty, '/')))
	    p++;
	else
	    p = user_tty;
	if (def_flag(I_TARGETPW))
	    len = easprintf(timestampfile, "%s/%s/%s:%s", dirparent, user_name,
		p, *user_runas);
	else
	    len = easprintf(timestampfile, "%s/%s/%s", dirparent, user_name, p);
	if (len >= MAXPATHLEN)
	    log_error(0, "timestamp path too long: %s", timestampfile);
    } else if (def_flag(I_TARGETPW)) {
	len = easprintf(timestampfile, "%s/%s/%s", dirparent, user_name,
	    *user_runas);
	if (len >= MAXPATHLEN)
	    log_error(0, "timestamp path too long: %s", timestampfile);
    } else
	*timestampfile = NULL;
}

/*
 * Check the timestamp file and directory and return their status.
 */
static int
timestamp_status(timestampdir, timestampfile, user, make_dirs)
    char *timestampdir;
    char *timestampfile;
    char *user;
    int make_dirs;
{
    struct stat sb;
    time_t now;
    char *dirparent = def_str(I_TIMESTAMPDIR);
    int status = TS_ERROR;		/* assume the worst */

    if (timestamp_uid != 0)
	set_perms(PERM_TIMESTAMP);

    /*
     * Sanity check dirparent and make it if it doesn't already exist.
     * We start out assuming the worst (that the dir is not sane) and
     * if it is ok upgrade the status to ``no timestamp file''.
     * Note that we don't check the parent(s) of dirparent for
     * sanity since the sudo dir is often just located in /tmp.
     */
    if (lstat(dirparent, &sb) == 0) {
	if (!S_ISDIR(sb.st_mode))
	    log_error(NO_EXIT, "%s exists but is not a directory (0%o)",
		dirparent, sb.st_mode);
	else if (sb.st_uid != timestamp_uid)
	    log_error(NO_EXIT, "%s owned by uid %lu, should be uid %lu",
		dirparent, (unsigned long) sb.st_uid,
		(unsigned long) timestamp_uid);
	else if ((sb.st_mode & 0000022))
	    log_error(NO_EXIT,
		"%s writable by non-owner (0%o), should be mode 0700",
		dirparent, sb.st_mode);
	else {
	    if ((sb.st_mode & 0000777) != 0700)
		(void) chmod(dirparent, 0700);
	    status = TS_MISSING;
	}
    } else if (errno != ENOENT) {
	log_error(NO_EXIT|USE_ERRNO, "can't stat %s", dirparent);
    } else {
	/* No dirparent, try to make one. */
	if (make_dirs) {
	    if (mkdir(dirparent, S_IRWXU))
		log_error(NO_EXIT|USE_ERRNO, "can't mkdir %s",
		    dirparent);
	    else
		status = TS_MISSING;
	}
    }
    if (status == TS_ERROR) {
	if (timestamp_uid != 0)
	    set_perms(PERM_ROOT);
	return(status);
    }

    /*
     * Sanity check the user's ticket dir.  We start by downgrading
     * the status to TS_ERROR.  If the ticket dir exists and is sane
     * this will be upgraded to TS_OLD.  If the dir does not exist,
     * it will be upgraded to TS_MISSING.
     */
    status = TS_ERROR;			/* downgrade status again */
    if (lstat(timestampdir, &sb) == 0) {
	if (!S_ISDIR(sb.st_mode)) {
	    if (S_ISREG(sb.st_mode)) {
		/* convert from old style */
		if (unlink(timestampdir) == 0)
		    status = TS_MISSING;
	    } else
		log_error(NO_EXIT, "%s exists but is not a directory (0%o)",
		    timestampdir, sb.st_mode);
	} else if (sb.st_uid != timestamp_uid)
	    log_error(NO_EXIT, "%s owned by uid %lu, should be uid %lu",
		timestampdir, (unsigned long) sb.st_uid,
		(unsigned long) timestamp_uid);
	else if ((sb.st_mode & 0000022))
	    log_error(NO_EXIT,
		"%s writable by non-owner (0%o), should be mode 0700",
		timestampdir, sb.st_mode);
	else {
	    if ((sb.st_mode & 0000777) != 0700)
		(void) chmod(timestampdir, 0700);
	    status = TS_OLD;		/* do date check later */
	}
    } else if (errno != ENOENT) {
	log_error(NO_EXIT|USE_ERRNO, "can't stat %s", timestampdir);
    } else
	status = TS_MISSING;

    /*
     * If there is no user ticket dir, AND we are in tty ticket mode,
     * AND the make_dirs flag is set, create the user ticket dir.
     */
    if (status == TS_MISSING && timestampfile && make_dirs) {
	if (mkdir(timestampdir, S_IRWXU) == -1) {
	    status = TS_ERROR;
	    log_error(NO_EXIT|USE_ERRNO, "can't mkdir %s", timestampdir);
	}
    }

    /*
     * Sanity check the tty ticket file if it exists.
     */
    if (timestampfile && status != TS_ERROR) {
	if (status != TS_MISSING)
	    status = TS_NOFILE;			/* dir there, file missing */
	if (lstat(timestampfile, &sb) == 0) {
	    if (!S_ISREG(sb.st_mode)) {
		status = TS_ERROR;
		log_error(NO_EXIT, "%s exists but is not a regular file (0%o)",
		    timestampfile, sb.st_mode);
	    } else {
		/* If bad uid or file mode, complain and kill the bogus file. */
		if (sb.st_uid != timestamp_uid) {
		    log_error(NO_EXIT,
			"%s owned by uid %ud, should be uid %lu",
			timestampfile, (unsigned long) sb.st_uid,
			(unsigned long) timestamp_uid);
		    (void) unlink(timestampfile);
		} else if ((sb.st_mode & 0000022)) {
		    log_error(NO_EXIT,
			"%s writable by non-owner (0%o), should be mode 0600",
			timestampfile, sb.st_mode);
		    (void) unlink(timestampfile);
		} else {
		    /* If not mode 0600, fix it. */
		    if ((sb.st_mode & 0000777) != 0600)
			(void) chmod(timestampfile, 0600);

		    status = TS_OLD;	/* actually check mtime below */
		}
	    }
	} else if (errno != ENOENT) {
	    log_error(NO_EXIT|USE_ERRNO, "can't stat %s", timestampfile);
	    status = TS_ERROR;
	}
    }

    /*
     * If the file/dir exists, check its mtime.
     */
    if (status == TS_OLD) {
	/* Negative timeouts only expire manually (sudo -k). */
	if (def_ival(I_TIMESTAMP_TIMEOUT) < 0 && sb.st_mtime != 0)
	    status = TS_CURRENT;
	else {
	    now = time(NULL);
	    if (def_ival(I_TIMESTAMP_TIMEOUT) && 
		now - sb.st_mtime < 60 * def_ival(I_TIMESTAMP_TIMEOUT)) {
		/*
		 * Check for bogus time on the stampfile.  The clock may
		 * have been set back or someone could be trying to spoof us.
		 */
		if (sb.st_mtime > now + 60 * def_ival(I_TIMESTAMP_TIMEOUT) * 2) {
		    log_error(NO_EXIT,
			"timestamp too far in the future: %20.20s",
			4 + ctime(&sb.st_mtime));
		    if (timestampfile)
			(void) unlink(timestampfile);
		    else
			(void) rmdir(timestampdir);
		    status = TS_MISSING;
		} else
		    status = TS_CURRENT;
	    }
	}
    }

    if (timestamp_uid != 0)
	set_perms(PERM_ROOT);
    return(status);
}

/*
 * Remove the timestamp ticket file/dir.
 */
void
remove_timestamp(remove)
    int remove;
{
    char *timestampdir;
    char *timestampfile;
    char *ts;
    int status;

    build_timestamp(&timestampdir, &timestampfile);
    status = timestamp_status(timestampdir, timestampfile, user_name, FALSE);
    if (status == TS_OLD || status == TS_CURRENT) {
	ts = timestampfile ? timestampfile : timestampdir;
	if (remove) {
	    if (timestampfile)
		status = unlink(timestampfile);
	    else
		status = rmdir(timestampdir);
	    if (status == -1 && errno != ENOENT) {
		log_error(NO_EXIT, "can't remove %s (%s), will reset to Epoch",
		    ts, strerror(errno));
		remove = FALSE;
	    }
	}
	if (!remove && touch(ts, 0) == -1)
	    err(1, "can't reset %s to Epoch", ts);
    }

    free(timestampdir);
    if (timestampfile)
	free(timestampfile);
}
