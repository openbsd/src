/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
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
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#include <err.h>
#include <agetarg.h>

#include <roken.h>

RCSID("$KTH: apwd.c,v 1.10 2000/10/03 00:33:12 lha Exp $");

static int verbose_flag;
static FILE *verbose_fp = NULL;

static int
initial_string (char **buf, size_t *size, size_t new_size)
{
    char *tmp = malloc (new_size);

    if (tmp == NULL)
	return -1;
    *buf  = tmp;
    *size = new_size;
    return 0;
}

static int
expand_string (char **buf, size_t *size, size_t new_size)
{
    char *tmp = realloc (*buf, new_size);

    if (tmp == NULL)
	return -1;
    *buf  = tmp;
    *size = new_size;
    return 0;
}

/*
 * Verify that the dynamically allocated string `buf' of length
 * `size', has room for `len' bytes.  Returns -1 if realloc fails.
 */

static int
guarantee_room (char **buf, size_t *size, size_t len)
{
    if (*size > len)
	return 0;

    return expand_string (buf, size, min(*size * 2, len));
}

static char *
getcwd_classic (char *buf, size_t size)
{
    int dynamic_buf = 0;
    struct stat root_sb, dot_sb, dotdot_sb;
    char *work_string;
    size_t work_length;
    char slash_dot_dot[] = "/..";
    char *curp;
    char *endp;
    DIR *dir = NULL;

    if (initial_string (&work_string, &work_length, MAXPATHLEN) < 0)
	return NULL;

    if (buf == NULL) {
	dynamic_buf = 1;
	if (initial_string (&buf, &size, MAXPATHLEN) < 0) {
	    free (work_string);
	    return NULL;
	}
    }

    endp = curp = buf + size - 1;

    if (lstat (".", &dot_sb) < 0)
	goto err_ret;
    if (lstat ("/", &root_sb) < 0)
	goto err_ret;
    strcpy (work_string, "..");
    fprintf (verbose_fp, "\".\" is (%u, %u), \"/\" is (%u, %u)\n",
	     (unsigned)dot_sb.st_dev, (unsigned)dot_sb.st_ino,
	     (unsigned)root_sb.st_dev, (unsigned)root_sb.st_ino);

    while (dot_sb.st_dev != root_sb.st_dev
	   || dot_sb.st_ino != root_sb.st_ino) {
	struct dirent *dp;
	int found = 0;
	int change_dev = 0;
	int pattern_len = strlen (work_string);

	if (lstat (work_string, &dotdot_sb) < 0)
	    goto err_ret;
	fprintf (verbose_fp, "\"..\" is (%u, %u)\n",
		 (unsigned)dotdot_sb.st_dev, (unsigned)dotdot_sb.st_ino);
	if (dot_sb.st_dev != dotdot_sb.st_dev)
	    change_dev = 1;
	dir = opendir (work_string);
	if (dir == NULL)
	    goto err_ret;
	while ((dp = readdir (dir)) != NULL) {
	    size_t name_len = strlen (dp->d_name);

	    if (change_dev) {
		struct stat sb;

		if (guarantee_room (&work_string, &work_length,
				    pattern_len + name_len + 2) < 0) {
		    goto err_ret;
		}

		strcat (work_string, "/");
		strcat (work_string, dp->d_name);

		if (lstat (work_string, &sb) < 0) {
		    goto err_ret;
		}
		if (sb.st_dev == dot_sb.st_dev
		    && sb.st_ino == dot_sb.st_ino) {
		    fprintf (verbose_fp, "\"%s\" found\n", work_string);
		    found = 1;
		}
		work_string[pattern_len] = '\0';
	    } else if (dp->d_ino == dot_sb.st_ino) {
		fprintf (verbose_fp, "\"%s\" found\n", dp->d_name);
		found = 1;
	    }

	    if (found) {
		while (buf + name_len >= curp) {
		    size_t old_len;

		    if (!dynamic_buf) {
			errno = ERANGE;
			goto err_ret;
		    }
		    old_len = endp - curp + 1;
		    if (expand_string (&buf, &size, size * 2) < 0)
			goto err_ret;
		    memmove (buf + size - old_len,
			     buf + size / 2 - old_len,
			     old_len);
		    endp = buf + size - 1;
		    curp = endp - old_len + 1;
		}
		memcpy (curp - name_len, dp->d_name, name_len);
		curp[-(name_len + 1)] = '/';
		curp -= name_len + 1;
		break;
	    }
	}
	closedir (dir);
	dir = NULL;

	if (!found)
	    goto err_ret;

	dot_sb = dotdot_sb;
	if (guarantee_room (&work_string, &work_length,
			    pattern_len + strlen(slash_dot_dot) + 1) < 0)
	    goto err_ret;
	strcat (work_string, slash_dot_dot);
    }
    if (curp == endp) {
	while (buf >= curp) {
	    if (!dynamic_buf) {
		errno = ERANGE;
		goto err_ret;
	    }
	    if (expand_string (&buf, &size, size * 2) < 0)
		goto err_ret;
	}
	*--curp = '/';
    }
    *endp = '\0';
    memmove (buf, curp, endp - curp + 1);
    free (work_string);
    return buf;

err_ret:
    if (dir)
	closedir (dir);
    if (dynamic_buf)
	free (buf);
    free (work_string);
    return NULL;
}

#if linux

static char *
getcwd_proc (char *buf, size_t size)
{
    int dynamic_buf = 0;

    if (buf == NULL) {
	dynamic_buf = 1;
	if (initial_string (&buf, &size, MAXPATHLEN) < 0)
	    return NULL;
    } else if (size <= 1) {
	errno = ERANGE;
	return NULL;
    }

    for (;;) {
	int ret;

	ret = readlink ("/proc/self/cwd", buf, size);
	if (ret == -1)
	    goto err_ret;
	if (buf[0] != '/') {
	    errno = EINVAL;
	    goto err_ret;
	}
	if (buf[ret-1] != '\0' && ret >= size) {
	    if (!dynamic_buf) {
		errno = ERANGE;
		goto err_ret;
	    }
	    if (expand_string (&buf, &size, size * 2) < 0)
		goto err_ret;
	} else {
	    if (buf[ret - 1] != '\0')
		buf[ret] = '\0';
	    return buf;
	}
    }
err_ret:
    if (dynamic_buf)
	free (buf);
    return NULL;
}

#endif /* linux */

static int
test_1(char *(*func)(char *, size_t), const char *func_name, int init_size)
{
    char real_buf[2048];
    char buf[2048], *buf2, buf3[4711];
    int i;
    int ret = 0;
    int three_done = 1;

    if (getcwd (real_buf, sizeof(real_buf)) == NULL) {
	fprintf (verbose_fp, "getcwd(buf, %u) failed\n", sizeof(real_buf));
	ret = 1;
    }
    if (func (buf, sizeof(buf)) == NULL) {
	fprintf (verbose_fp, "%s(buf, %u) failed\n", func_name, sizeof(buf));
	ret = 1;
    } else {
	fprintf (verbose_fp, "first *%s*\n", buf);
	if (strcmp (real_buf, buf) != 0) {
	    fprintf (verbose_fp, "first comparison failed: *%s* != *%s*\n",
		     real_buf, buf);
	    ret = 1;
	}
    }

    buf2 = func (NULL, 0);
    if (buf2 == NULL) {
	fprintf (verbose_fp, "%s(NULL, 0) failed\n", func_name);
	ret = 1;
    } else {
	fprintf (verbose_fp, "second *%s*\n", buf2);
	if (strcmp (real_buf, buf2) != 0) {
	    fprintf (verbose_fp, "second comparison failed: *%s* != *%s*\n",
		     real_buf, buf2);
	    ret = 1;
	}
	free (buf2);
    }

    for (i = init_size; i < sizeof(buf3); ++i) {
	memset (buf3, '\x01', sizeof(buf3));
	if (func (buf3, i) == NULL) {
	    if (errno != ERANGE) {
		fprintf (verbose_fp, "%s(buf,%u) call failed\n", func_name, i);
		three_done = 0;
		break;
	    }
	} else {
	    int j;

	    for (j = i; j < sizeof(buf3); ++j)
		if (buf3[j] != '\x01') {
		    fprintf (verbose_fp, "buffer was overwritten at %d\n", j);
		    three_done = 0;
		    break;
		}
	    break;
	}
    }

    if (three_done) {
	fprintf (verbose_fp, "third *%s*\n", buf3);
	if (strcmp (real_buf, buf3) != 0) {
	    fprintf (verbose_fp, "third comparison failed: *%s* != *%s*\n",
		     real_buf, buf3);
	    ret = 1;
	} else if (strlen (buf3) + 1 != i
		   && strlen (buf3) + 1 >= init_size) {
	    fprintf (verbose_fp, "wrong len in third call: %d != %d\n",
		     strlen(buf3) + 1, i);
	    ret = 1;
	}
    } else {
	ret = 1;
    }

    return ret;
}

static int
test_it(char *(*func)(char *, size_t), const char *name, int init_size)
{
    int ret;

    fprintf (verbose_fp, "testing %s (initial size %d)\n", name, init_size);
    ret = test_1 (func, name, init_size);
    if (ret)
	fprintf (verbose_fp, "FAILED!\n");
    else
	fprintf (verbose_fp, "passed\n");
    return ret;
}

#ifdef linux
#include <linux/unistd.h>
#endif

#ifdef __NR_getcwd

#define __NR_sys_getcwd __NR_getcwd

static
_syscall2(int, sys_getcwd, char *, buf, size_t, size)

static char *
getcwd_syscall (char *buf, size_t size)
{
    int dynamic_buf = 0;

    if (buf == NULL) {
	dynamic_buf = 1;
	if (initial_string (&buf, &size, MAXPATHLEN) < 0)
	    return NULL;
    }

    for (;;) {
	int ret;

	ret = sys_getcwd (buf, size);
	if (ret >= 0)
	    return buf;
	else if (errno == ERANGE) {
	    if (!dynamic_buf)
		return NULL;
	    if (expand_string (&buf, &size, size * 2) < 0)
		return NULL;
	} else
	    return NULL;
    }
}

#endif

static int help_flag;

static struct agetargs args[] = {
    {"verbose", 'v',	aarg_flag,	&verbose_flag,	"verbose",	NULL},
    {"help",	0,	aarg_flag,	&help_flag,	NULL,		NULL},
    {NULL,	0,	aarg_end,	NULL,		NULL,		NULL}
};

static void
usage (int exit_val)
{
    aarg_printusage (args, NULL, "", AARG_GNUSTYLE);
}

int
main(int argc, char **argv)
{
    int ret = 0;
    int optind = 0;

    set_progname (argv[0]);

    verbose_flag = getenv ("VERBOSE") != NULL;

    if (agetarg (args, argc, argv, &optind, AARG_GNUSTYLE))
	usage (1);

    argc -= optind;
    argv += optind;

    if (argc != 0)
	usage (1);
    if (help_flag)
	usage (0);

    verbose_fp = fdopen (4, "w");
    if (verbose_fp == NULL) {
	verbose_fp = fopen ("/dev/null", "w");
	if (verbose_fp == NULL)
	    err (1, "fopen");
    }

    ret += test_it (getcwd, "getcwd", 3);
#ifdef __NR_getcwd
    ret += test_it (getcwd_syscall, "getcwd_syscall", 3);
#endif
    ret += test_it (getcwd_classic, "getcwd_classic", 0);
#if linux
    ret += test_it (getcwd_proc, "getcwd_proc", 0);
#endif
    return ret;
}
