/*
 * Copyright (c) 1993-1996,1998-2005, 2007-2009
 *	Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */

#include <config.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#ifndef __TANDEM
# include <sys/file.h>
#endif
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
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#ifndef HAVE_TIMESPEC
# include <emul/timespec.h>
#endif

#include "sudo.h"

/* Status codes for timestamp_status() */
#define TS_CURRENT		0
#define TS_OLD			1
#define TS_MISSING		2
#define TS_NOFILE		3
#define TS_ERROR		4

/* Flags for timestamp_status() */
#define TS_MAKE_DIRS		1
#define TS_REMOVE		2

static void  build_timestamp	__P((char **, char **));
static int   timestamp_status	__P((char *, char *, char *, int));
static char *expand_prompt	__P((char *, char *, char *));
static void  lecture		__P((int));
static void  update_timestamp	__P((char *, char *));

/*
 * This function only returns if the user can successfully
 * verify who he/she is.
 */
void
check_user(validated, mode)
    int validated;
    int mode;
{
    char *timestampdir = NULL;
    char *timestampfile = NULL;
    char *prompt;
    int status;

    if (mode & MODE_INVALIDATE) {
	/* do not check or update timestamp */
	status = TS_ERROR;
    } else {
	if (user_uid == 0 || user_uid == runas_pw->pw_uid || user_is_exempt())
	    return;

	build_timestamp(&timestampdir, &timestampfile);
	status = timestamp_status(timestampdir, timestampfile, user_name,
	TS_MAKE_DIRS);
    }
    if (status != TS_CURRENT || ISSET(validated, FLAG_CHECK_USER)) {
	/* Bail out if we are non-interactive and a password is required */
	if (ISSET(mode, MODE_NONINTERACTIVE))
	    errorx(1, "sorry, a password is required to run %s", getprogname());

	/* If user specified -A, make sure we have an askpass helper. */
	if (ISSET(tgetpass_flags, TGP_ASKPASS)) {
	    if (user_askpass == NULL)
		log_error(NO_MAIL,
		    "no askpass program specified, try setting SUDO_ASKPASS");
	} else if (!ISSET(tgetpass_flags, TGP_STDIN)) {
	    /* If no tty but DISPLAY is set, use askpass if we have it. */
	    if (!user_ttypath && !tty_present()) {
		if (user_askpass && user_display && *user_display != '\0') {
		    SET(tgetpass_flags, TGP_ASKPASS);
		} else if (!def_visiblepw) {
		    log_error(NO_MAIL,
			"no tty present and no askpass program specified");
		}
	    }
	}

	if (!ISSET(tgetpass_flags, TGP_ASKPASS))
	    lecture(status);

	/* Expand any escapes in the prompt. */
	prompt = expand_prompt(user_prompt ? user_prompt : def_passprompt,
	    user_name, user_shost);

	verify_user(auth_pw, prompt);
    }
    /* Only update timestamp if user was validated. */
    if (status != TS_ERROR && ISSET(validated, VALIDATE_OK))
	update_timestamp(timestampdir, timestampfile);
    efree(timestampdir);
    efree(timestampfile);
}

/*
 * Standard sudo lecture.
 */
static void
lecture(status)
    int status;
{
    FILE *fp;
    char buf[BUFSIZ];
    ssize_t nread;

    if (def_lecture == never ||
	(def_lecture == once && status != TS_MISSING && status != TS_ERROR))
	return;

    if (def_lecture_file && (fp = fopen(def_lecture_file, "r")) != NULL) {
	while ((nread = fread(buf, sizeof(char), sizeof(buf), fp)) != 0)
	    fwrite(buf, nread, 1, stderr);
	fclose(fp);
    } else {
	(void) fputs("\n\
We trust you have received the usual lecture from the local System\n\
Administrator. It usually boils down to these three things:\n\
\n\
    #1) Respect the privacy of others.\n\
    #2) Think before you type.\n\
    #3) With great power comes great responsibility.\n\n",
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
    if (touch(-1, timestampfile ? timestampfile : timestampdir, NULL) == -1) {
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
		case 'p':
		    p++;
		    if (def_rootpw)
			    len += 2;
		    else if (def_targetpw || def_runaspw)
			    len += strlen(runas_pw->pw_name) - 2;
		    else
			    len += strlen(user_name) - 2;
		    subst = 1;
		    break;
		case 'u':
		    p++;
		    len += strlen(user_name) - 2;
		    subst = 1;
		    break;
		case 'U':
		    p++;
		    len += strlen(runas_pw->pw_name) - 2;
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
		    case 'p':
			p++;
			if (def_rootpw)
				n = strlcpy(np, "root", np - endp);
			else if (def_targetpw || def_runaspw)
				n = strlcpy(np, runas_pw->pw_name, np - endp);
			else
				n = strlcpy(np, user_name, np - endp);
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
			n = strlcpy(np,  runas_pw->pw_name, np - endp);
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
    errorx(1, "internal error, expand_prompt() overflow");
}

/*
 * Checks if the user is exempt from supplying a password.
 */
int
user_is_exempt()
{
    struct group *grp;
    char **gr_mem;

    if (!def_exempt_group)
	return(FALSE);

    if (!(grp = sudo_getgrnam(def_exempt_group)))
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

    dirparent = def_timestampdir;
    len = easprintf(timestampdir, "%s/%s", dirparent, user_name);
    if (len >= PATH_MAX)
	log_error(0, "timestamp path too long: %s", *timestampdir);

    /*
     * Timestamp file may be a file in the directory or NUL to use
     * the directory as the timestamp.
     */
    if (def_tty_tickets) {
	char *p;

	if ((p = strrchr(user_tty, '/')))
	    p++;
	else
	    p = user_tty;
	if (def_targetpw)
	    len = easprintf(timestampfile, "%s/%s/%s:%s", dirparent, user_name,
		p, runas_pw->pw_name);
	else
	    len = easprintf(timestampfile, "%s/%s/%s", dirparent, user_name, p);
	if (len >= PATH_MAX)
	    log_error(0, "timestamp path too long: %s", *timestampfile);
    } else if (def_targetpw) {
	len = easprintf(timestampfile, "%s/%s/%s", dirparent, user_name,
	    runas_pw->pw_name);
	if (len >= PATH_MAX)
	    log_error(0, "timestamp path too long: %s", *timestampfile);
    } else
	*timestampfile = NULL;
}

/*
 * Check the timestamp file and directory and return their status.
 */
static int
timestamp_status(timestampdir, timestampfile, user, flags)
    char *timestampdir;
    char *timestampfile;
    char *user;
    int flags;
{
    struct stat sb;
    time_t now;
    char *dirparent = def_timestampdir;
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
		dirparent, (unsigned int) sb.st_mode);
	else if (sb.st_uid != timestamp_uid)
	    log_error(NO_EXIT, "%s owned by uid %lu, should be uid %lu",
		dirparent, (unsigned long) sb.st_uid,
		(unsigned long) timestamp_uid);
	else if ((sb.st_mode & 0000022))
	    log_error(NO_EXIT,
		"%s writable by non-owner (0%o), should be mode 0700",
		dirparent, (unsigned int) sb.st_mode);
	else {
	    if ((sb.st_mode & 0000777) != 0700)
		(void) chmod(dirparent, 0700);
	    status = TS_MISSING;
	}
    } else if (errno != ENOENT) {
	log_error(NO_EXIT|USE_ERRNO, "can't stat %s", dirparent);
    } else {
	/* No dirparent, try to make one. */
	if (ISSET(flags, TS_MAKE_DIRS)) {
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
		    timestampdir, (unsigned int) sb.st_mode);
	} else if (sb.st_uid != timestamp_uid)
	    log_error(NO_EXIT, "%s owned by uid %lu, should be uid %lu",
		timestampdir, (unsigned long) sb.st_uid,
		(unsigned long) timestamp_uid);
	else if ((sb.st_mode & 0000022))
	    log_error(NO_EXIT,
		"%s writable by non-owner (0%o), should be mode 0700",
		timestampdir, (unsigned int) sb.st_mode);
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
     * AND the TS_MAKE_DIRS flag is set, create the user ticket dir.
     */
    if (status == TS_MISSING && timestampfile && ISSET(flags, TS_MAKE_DIRS)) {
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
		    timestampfile, (unsigned int) sb.st_mode);
	    } else {
		/* If bad uid or file mode, complain and kill the bogus file. */
		if (sb.st_uid != timestamp_uid) {
		    log_error(NO_EXIT,
			"%s owned by uid %lu, should be uid %lu",
			timestampfile, (unsigned long) sb.st_uid,
			(unsigned long) timestamp_uid);
		    (void) unlink(timestampfile);
		} else if ((sb.st_mode & 0000022)) {
		    log_error(NO_EXIT,
			"%s writable by non-owner (0%o), should be mode 0600",
			timestampfile, (unsigned int) sb.st_mode);
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
     * If the file/dir exists and we are not removing it, check its mtime.
     */
    if (status == TS_OLD && !ISSET(flags, TS_REMOVE)) {
	/* Negative timeouts only expire manually (sudo -k). */
	if (def_timestamp_timeout < 0 && sb.st_mtime != 0)
	    status = TS_CURRENT;
	else {
	    /* XXX - should use timespec here */
	    now = time(NULL);
	    if (def_timestamp_timeout &&
		now - sb.st_mtime < 60 * def_timestamp_timeout) {
		/*
		 * Check for bogus time on the stampfile.  The clock may
		 * have been set back or someone could be trying to spoof us.
		 */
		if (sb.st_mtime > now + 60 * def_timestamp_timeout * 2) {
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
    struct timespec ts;
    char *timestampdir, *timestampfile, *path;
    int status;

    build_timestamp(&timestampdir, &timestampfile);
    status = timestamp_status(timestampdir, timestampfile, user_name,
	TS_REMOVE);
    if (status == TS_OLD || status == TS_CURRENT) {
	path = timestampfile ? timestampfile : timestampdir;
	if (remove) {
	    if (timestampfile)
		status = unlink(timestampfile);
	    else
		status = rmdir(timestampdir);
	    if (status == -1 && errno != ENOENT) {
		log_error(NO_EXIT, "can't remove %s (%s), will reset to Epoch",
		    path, strerror(errno));
		remove = FALSE;
	    }
	} else {
	    timespecclear(&ts);
	    if (touch(-1, path, &ts) == -1)
		error(1, "can't reset %s to Epoch", path);
	}
    }

    efree(timestampdir);
    efree(timestampfile);
}
