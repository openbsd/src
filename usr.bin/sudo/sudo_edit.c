/*
 * Copyright (c) 2004 Todd C. Miller <Todd.Miller@courtesan.com>
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

#include "config.h"

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
#ifdef HAVE_ERR_H
# include <err.h>
#else
# include "emul/err.h"
#endif /* HAVE_ERR_H */
#include <ctype.h>
#include <pwd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

#include "sudo.h"

#ifndef lint
static const char rcsid[] = "$Sudo: sudo_edit.c,v 1.16 2004/09/15 16:16:20 millert Exp $";
#endif /* lint */

extern sigaction_t saved_sa_int, saved_sa_quit, saved_sa_tstp, saved_sa_chld;

/*
 * Wrapper to allow users to edit privileged files with their own uid.
 */
int sudo_edit(argc, argv)
    int argc;
    char **argv;
{
    ssize_t nread, nwritten;
    pid_t kidpid, pid;
    const char *tmpdir;
    char **nargv, **ap, *editor, *cp;
    char buf[BUFSIZ];
    int i, ac, ofd, tfd, nargc, rval, tmplen;
    sigaction_t sa;
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
     * For each file specified by the user, make a temporary version
     * and copy the contents of the original to it.
     * XXX - It would be nice to lock the original files but that means
     *       keeping an extra fd open for each file.
     */
    tf = emalloc2(argc - 1, sizeof(*tf));
    memset(tf, 0, (argc - 1) * sizeof(*tf));
    for (i = 0, ap = argv + 1; i < argc - 1 && *ap != NULL; i++, ap++) {
	set_perms(PERM_RUNAS);
	ofd = open(*ap, O_RDONLY, 0644);
	if (ofd != -1) {
#ifdef HAVE_FSTAT
	    if (fstat(ofd, &sb) != 0) {
#else
	    if (stat(tf[i].ofile, &sb) != 0) {
#endif
		close(ofd);	/* XXX - could reset errno */
		ofd = -1;
	    }
	}
	set_perms(PERM_ROOT);
	if (ofd == -1) {
	    if (errno != ENOENT) {
		warn("%s", *ap);
		argc--;
		i--;
		continue;
	    }
	    memset(&sb, 0, sizeof(sb));
	} else if (!S_ISREG(sb.st_mode)) {
	    warnx("%s: not a regular file", *ap);
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
	    warn("mkstemp");
	    goto cleanup;
	}
	if (ofd != -1) {
	    while ((nread = read(ofd, buf, sizeof(buf))) != 0) {
		if ((nwritten = write(tfd, buf, nread)) != nread) {
		    if (nwritten == -1)
			warn("%s", tf[i].tfile);
		    else
			warnx("%s: short write", tf[i].tfile);
		    goto cleanup;
		}
	    }
	    close(ofd);
	}
#ifdef HAVE_FSTAT
	/*
	 * If we are unable to set the mtime on the temp file to the value
	 * of the original file just make the stashed mtime match the temp
	 * file's mtime.  It is better than nothing and we only use the info
	 * to determine whether or not a file has been modified.
	 */
	if (touch(tfd, NULL, &tf[i].omtim) == -1) {
	    if (fstat(tfd, &sb) == 0) {
		tf[i].omtim.tv_sec = mtim_getsec(sb);
		tf[i].omtim.tv_nsec = mtim_getnsec(sb);
	    }
	    /* XXX - else error? */
	}
#endif
	close(tfd);
    }
    if (argc == 1)
	return(1);			/* no files readable, you lose */

    /*
     * Determine which editor to use.  We don't bother restricting this
     * based on def_env_editor or def_editor since the editor runs with
     * the uid of the invoking user, not the runas (privileged) user.
     */
    if (((editor = getenv("VISUAL")) != NULL && *editor != '\0') ||
	((editor = getenv("EDITOR")) != NULL && *editor != '\0')) {
	editor = estrdup(editor);
    } else {
	editor = estrdup(def_editor);
	if ((cp = strchr(editor, ':')) != NULL)
	    *cp = '\0';			/* def_editor could be a path */
    }

    /*
     * Allocate space for the new argument vector and fill it in.
     * The EDITOR and VISUAL environment variables may contain command
     * line args so look for those and alloc space for them too.
     */
    nargc = argc;
    for (cp = editor + 1; *cp != '\0'; cp++) {
	if (isblank((unsigned char)cp[0]) && !isblank((unsigned char)cp[-1]))
	    nargc++;
    }
    nargv = (char **) emalloc2(nargc + 1, sizeof(char *));
    ac = 0;
    for ((cp = strtok(editor, " \t")); cp != NULL; (cp = strtok(NULL, " \t")))
	nargv[ac++] = cp;
    for (i = 0; i < argc - 1 && ac < nargc; )
	nargv[ac++] = tf[i++].tfile;
    nargv[ac] = NULL;

    /* We wait for our own children and can be suspended. */
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = SIG_DFL;
    (void) sigaction(SIGCHLD, &sa, NULL);
    (void) sigaction(SIGTSTP, &saved_sa_tstp, NULL);

    /*
     * Fork and exec the editor with the invoking user's creds,
     * keeping track of the time spent in the editor.
     */
    gettime(&ts1);
    kidpid = fork();
    if (kidpid == -1) {
	warn("fork");
	goto cleanup;
    } else if (kidpid == 0) {
	/* child */
	(void) sigaction(SIGINT, &saved_sa_int, NULL);
	(void) sigaction(SIGQUIT, &saved_sa_quit, NULL);
	(void) sigaction(SIGCHLD, &saved_sa_chld, NULL);
	set_perms(PERM_FULL_USER);
	execvp(nargv[0], nargv);
	warn("unable to execute %s", nargv[0]);
	_exit(127);
    }

    /*
     * Wait for status from the child.  Most modern kernels
     * will not let an unprivileged child process send a
     * signal to its privileged parent to we have to request
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
	set_perms(PERM_USER);
	tfd = open(tf[i].tfile, O_RDONLY, 0644);
	set_perms(PERM_ROOT);
	if (tfd < 0) {
	    warn("unable to read %s", tf[i].tfile);
	    warnx("%s left unmodified", tf[i].ofile);
	    continue;
	}
#ifdef HAVE_FSTAT
	if (fstat(tfd, &sb) == 0) {
	    if (!S_ISREG(sb.st_mode)) {
		warnx("%s: not a regular file", tf[i].tfile);
		warnx("%s left unmodified", tf[i].ofile);
		continue;
	    }
	    if (tf[i].osize == sb.st_size &&
		tf[i].omtim.tv_sec == mtim_getsec(sb) &&
		tf[i].omtim.tv_nsec == mtim_getnsec(sb)) {
		/*
		 * If mtime and size match but the user spent no measurable
		 * time in the editor we can't tell if the file was changed.
		 */
		timespecsub(&ts1, &ts2, &ts2);
		if (timespecisset(&ts2)) {
		    warnx("%s unchanged", tf[i].ofile);
		    unlink(tf[i].tfile);
		    close(tfd);
		    continue;
		}
	    }
	}
#endif
	set_perms(PERM_RUNAS);
	ofd = open(tf[i].ofile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
	set_perms(PERM_ROOT);
	if (ofd == -1) {
	    warn("unable to write to %s", tf[i].ofile);
	    warnx("contents of edit session left in %s", tf[i].tfile);
	    close(tfd);
	    continue;
	}
	while ((nread = read(tfd, buf, sizeof(buf))) > 0) {
	    if ((nwritten = write(ofd, buf, nread)) != nread) {
		if (nwritten == -1)
		    warn("%s", tf[i].ofile);
		else
		    warnx("%s: short write", tf[i].ofile);
		break;
	    }
	}
	if (nread == 0) {
	    /* success, got EOF */
	    unlink(tf[i].tfile);
	} else if (nread < 0) {
	    warn("unable to read temporary file");
	    warnx("contents of edit session left in %s", tf[i].tfile);
	} else {
	    warn("unable to write to %s", tf[i].ofile);
	    warnx("contents of edit session left in %s", tf[i].tfile);
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
