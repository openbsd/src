/*
 * Copyright (c) 2004-2008 Todd C. Miller <Todd.Miller@courtesan.com>
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
 */

#include <config.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/socket.h>
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
#include <ctype.h>
#include <pwd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#if TIME_WITH_SYS_TIME
# include <time.h>
#endif
#ifndef HAVE_TIMESPEC
# include <emul/timespec.h>
#endif

#include "sudo.h"

extern sigaction_t saved_sa_int, saved_sa_quit, saved_sa_tstp;
extern char **environ;

static char *find_editor();

/*
 * Wrapper to allow users to edit privileged files with their own uid.
 */
int
sudo_edit(argc, argv, envp)
    int argc;
    char **argv;
    char **envp;
{
    ssize_t nread, nwritten;
    pid_t kidpid, pid;
    const char *tmpdir;
    char **nargv, **ap, *editor, *cp;
    char buf[BUFSIZ];
    int error, i, ac, ofd, tfd, nargc, rval, tmplen, wasblank;
    struct stat sb;
    struct timespec ts1, ts2;
    struct tempfile {
	char *tfile;
	char *ofile;
	struct timespec omtim;
	off_t osize;
    } *tf;

    /*
     * Find our temporary directory, one of /var/tmp, /usr/tmp, or /tmp
     */
    if (stat(_PATH_VARTMP, &sb) == 0 && S_ISDIR(sb.st_mode))
	tmpdir = _PATH_VARTMP;
#ifdef _PATH_USRTMP
    else if (stat(_PATH_USRTMP, &sb) == 0 && S_ISDIR(sb.st_mode))
	tmpdir = _PATH_USRTMP;
#endif
    else
	tmpdir = _PATH_TMP;
    tmplen = strlen(tmpdir);
    while (tmplen > 0 && tmpdir[tmplen - 1] == '/')
	tmplen--;

    /*
     * Close password, shadow, and group files before we try to open
     * user-specified files to prevent the opening of things like /dev/fd/4
     */
    sudo_endpwent();
    sudo_endgrent();

    /*
     * For each file specified by the user, make a temporary version
     * and copy the contents of the original to it.
     */
    tf = emalloc2(argc - 1, sizeof(*tf));
    zero_bytes(tf, (argc - 1) * sizeof(*tf));
    for (i = 0, ap = argv + 1; i < argc - 1 && *ap != NULL; i++, ap++) {
	error = -1;
	set_perms(PERM_RUNAS);
	if ((ofd = open(*ap, O_RDONLY, 0644)) != -1 || errno == ENOENT) {
	    if (ofd == -1) {
		zero_bytes(&sb, sizeof(sb));		/* new file */
		error = 0;
	    } else {
#ifdef HAVE_FSTAT
		error = fstat(ofd, &sb);
#else
		error = stat(tf[i].ofile, &sb);
#endif
	    }
	}
	set_perms(PERM_ROOT);
	if (error || (ofd != -1 && !S_ISREG(sb.st_mode))) {
	    if (error)
		warning("%s", *ap);
	    else
		warningx("%s: not a regular file", *ap);
	    if (ofd != -1)
		close(ofd);
	    argc--;
	    i--;
	    continue;
	}
	tf[i].ofile = *ap;
	tf[i].omtim.tv_sec = mtim_getsec(sb);
	tf[i].omtim.tv_nsec = mtim_getnsec(sb);
	tf[i].osize = sb.st_size;
	if ((cp = strrchr(tf[i].ofile, '/')) != NULL)
	    cp++;
	else
	    cp = tf[i].ofile;
	easprintf(&tf[i].tfile, "%.*s/%s.XXXXXXXX", tmplen, tmpdir, cp);
	set_perms(PERM_USER);
	tfd = mkstemp(tf[i].tfile);
	set_perms(PERM_ROOT);
	if (tfd == -1) {
	    warning("mkstemp");
	    goto cleanup;
	}
	if (ofd != -1) {
	    while ((nread = read(ofd, buf, sizeof(buf))) != 0) {
		if ((nwritten = write(tfd, buf, nread)) != nread) {
		    if (nwritten == -1)
			warning("%s", tf[i].tfile);
		    else
			warningx("%s: short write", tf[i].tfile);
		    goto cleanup;
		}
	    }
	    close(ofd);
	}
	/*
	 * We always update the stashed mtime because the time
	 * resolution of the filesystem the temporary file is on may
	 * not match that of the filesystem where the file to be edited
	 * resides.  It is OK if touch() fails since we only use the info
	 * to determine whether or not a file has been modified.
	 */
	(void) touch(tfd, NULL, &tf[i].omtim);
#ifdef HAVE_FSTAT
	error = fstat(tfd, &sb);
#else
	error = stat(tf[i].tfile, &sb);
#endif
	if (!error) {
	    tf[i].omtim.tv_sec = mtim_getsec(sb);
	    tf[i].omtim.tv_nsec = mtim_getnsec(sb);
	}
	close(tfd);
    }
    if (argc == 1)
	return(1);			/* no files readable, you lose */

    environ = envp;
    editor = find_editor();

    /*
     * Allocate space for the new argument vector and fill it in.
     * The EDITOR and VISUAL environment variables may contain command
     * line args so look for those and alloc space for them too.
     */
    nargc = argc;
    for (wasblank = FALSE, cp = editor; *cp != '\0'; cp++) {
	if (isblank((unsigned char) *cp))
	    wasblank = TRUE;
	else if (wasblank) {
	    wasblank = FALSE;
	    nargc++;
	}
    }
    nargv = (char **) emalloc2(nargc + 1, sizeof(char *));
    ac = 0;
    for ((cp = strtok(editor, " \t")); cp != NULL; (cp = strtok(NULL, " \t")))
	nargv[ac++] = cp;
    for (i = 0; i < argc - 1 && ac < nargc; )
	nargv[ac++] = tf[i++].tfile;
    nargv[ac] = NULL;

    /* Allow the editor to be suspended. */
    (void) sigaction(SIGTSTP, &saved_sa_tstp, NULL);

    /*
     * Fork and exec the editor with the invoking user's creds,
     * keeping track of the time spent in the editor.
     */
    gettime(&ts1);
    kidpid = fork();
    if (kidpid == -1) {
	warning("fork");
	goto cleanup;
    } else if (kidpid == 0) {
	/* child */
	(void) sigaction(SIGINT, &saved_sa_int, NULL);
	(void) sigaction(SIGQUIT, &saved_sa_quit, NULL);
	set_perms(PERM_FULL_USER);
	closefrom(def_closefrom);
	execvp(nargv[0], nargv);
	warning("unable to execute %s", nargv[0]);
	_exit(127);
    }

    /*
     * Wait for status from the child.  Most modern kernels
     * will not let an unprivileged child process send a
     * signal to its privileged parent so we have to request
     * status when the child is stopped and then send the
     * same signal to our own pid.
     */
    do {
#ifdef sudo_waitpid
        pid = sudo_waitpid(kidpid, &i, WUNTRACED);
#else
	pid = wait(&i);
#endif
	if (pid == kidpid) {
	    if (WIFSTOPPED(i))
		kill(getpid(), WSTOPSIG(i));
	    else
		break;
	}
    } while (pid != -1 || errno == EINTR);
    gettime(&ts2);
    if (pid == -1 || !WIFEXITED(i))
	rval = 1;
    else
	rval = WEXITSTATUS(i);

    /* Copy contents of temp files to real ones */
    for (i = 0; i < argc - 1; i++) {
	error = -1;
	set_perms(PERM_USER);
	if ((tfd = open(tf[i].tfile, O_RDONLY, 0644)) != -1) {
#ifdef HAVE_FSTAT
	    error = fstat(tfd, &sb);
#else
	    error = stat(tf[i].tfile, &sb);
#endif
	}
	set_perms(PERM_ROOT);
	if (error || !S_ISREG(sb.st_mode)) {
	    if (error)
		warning("%s", tf[i].tfile);
	    else
		warningx("%s: not a regular file", tf[i].tfile);
	    warningx("%s left unmodified", tf[i].ofile);
	    if (tfd != -1)
		close(tfd);
	    continue;
	}
	if (tf[i].osize == sb.st_size && tf[i].omtim.tv_sec == mtim_getsec(sb)
	    && tf[i].omtim.tv_nsec == mtim_getnsec(sb)) {
	    /*
	     * If mtime and size match but the user spent no measurable
	     * time in the editor we can't tell if the file was changed.
	     */
#ifdef HAVE_TIMESPECSUB2
	    timespecsub(&ts1, &ts2);
#else
	    timespecsub(&ts1, &ts2, &ts2);
#endif
	    if (timespecisset(&ts2)) {
		warningx("%s unchanged", tf[i].ofile);
		unlink(tf[i].tfile);
		close(tfd);
		continue;
	    }
	}
	set_perms(PERM_RUNAS);
	ofd = open(tf[i].ofile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
	set_perms(PERM_ROOT);
	if (ofd == -1) {
	    warning("unable to write to %s", tf[i].ofile);
	    warningx("contents of edit session left in %s", tf[i].tfile);
	    close(tfd);
	    continue;
	}
	while ((nread = read(tfd, buf, sizeof(buf))) > 0) {
	    if ((nwritten = write(ofd, buf, nread)) != nread) {
		if (nwritten == -1)
		    warning("%s", tf[i].ofile);
		else
		    warningx("%s: short write", tf[i].ofile);
		break;
	    }
	}
	if (nread == 0) {
	    /* success, got EOF */
	    unlink(tf[i].tfile);
	} else if (nread < 0) {
	    warning("unable to read temporary file");
	    warningx("contents of edit session left in %s", tf[i].tfile);
	} else {
	    warning("unable to write to %s", tf[i].ofile);
	    warningx("contents of edit session left in %s", tf[i].tfile);
	}
	close(ofd);
    }

    return(rval);
cleanup:
    /* Clean up temp files and return. */
    for (i = 0; i < argc - 1; i++) {
	if (tf[i].tfile != NULL)
	    unlink(tf[i].tfile);
    }
    return(1);
}

/*
 * Determine which editor to use.  We don't bother restricting this
 * based on def_env_editor or def_editor since the editor runs with
 * the uid of the invoking user, not the runas (privileged) user.
 */
static char *
find_editor()
{
    char *cp, *editor = NULL, **ev, *ev0[4];

    ev0[0] = "SUDO_EDITOR";
    ev0[1] = "VISUAL";
    ev0[2] = "EDITOR";
    ev0[3] = NULL;
    for (ev = ev0; *ev != NULL; ev++) {
	if ((editor = getenv(*ev)) != NULL && *editor != '\0') {
	    if ((cp = strrchr(editor, '/')) != NULL)
		cp++;
	    else
		cp = editor;
	    /* Ignore "sudoedit" and "sudo" to avoid an endless loop. */
	    if (strncmp(cp, "sudo", 4) != 0 ||
		(cp[4] != ' ' && cp[4] != '\0' && strcmp(cp + 4, "edit") != 0)) {
		editor = estrdup(editor);
		break;
	    }
	}
	editor = NULL;
    }
    if (editor == NULL) {
	editor = estrdup(def_editor);
	if ((cp = strchr(editor, ':')) != NULL)
	    *cp = '\0';			/* def_editor could be a path */
    }
    return(editor);
}
