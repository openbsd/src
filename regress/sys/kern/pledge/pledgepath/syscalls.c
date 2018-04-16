/*	$OpenBSD: syscalls.c,v 1.8 2018/04/16 14:28:44 beck Exp $	*/

/*
 * Copyright (c) 2017 Bob Beck <beck@openbsd.org>
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <err.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/errno.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/mount.h>

pid_t child;
char pp_dir1[] = "/tmp/ppdir1.XXXXXX"; /* pledgepathed */
char pp_dir2[] = "/tmp/ppdir2.XXXXXX"; /* not pledgepathed */
char pp_file1[] = "/tmp/ppfile1.XXXXXX"; /* pledgepathed */
char pp_file2[] = "/tmp/ppfile2.XXXXXX"; /* not pledgepathed */

#define PP_SHOULD_SUCCEED(A, B) do {						\
	if (A) {								\
		err(1, "%s:%d - %s", __FILE__, __LINE__, B);			\
	}									\
} while (0)

#define PP_SHOULD_FAIL(A, B) do {						\
	if (A) {				 				\
		if (do_pp && errno != ENOENT)		     			\
			err(1, "%s:%d - %s", __FILE__, __LINE__, B);		\
	} else {								\
		if (do_pp)							\
			errx(1, "%s:%d - %s worked when it should not "		\
			    "have",  __FILE__, __LINE__, B);			\
	}									\
} while(0)

static void
do_pledgepath(void)
{
	if (pledgepath(pp_dir1, 0) == -1)
                err(1, "%s:%d - pledgepath", __FILE__, __LINE__);
	if (pledgepath(pp_file1, 0) == -1)
                err(1, "%s:%d - pledgepath", __FILE__, __LINE__);
}

static int
runcompare(int (*func)(int))
{
	int ppath = 0, nonppath = 0, status;
	pid_t pid = fork();
	if (pid == 0) {
		exit(func(0));
	}
	status = 0;
	waitpid(pid, &status, 0);
	if (WIFEXITED(status))
		nonppath = WEXITSTATUS(status);
	pid = fork();
	if (pid == 0) {
		exit(func(1));
	}
	status = 0;
	waitpid(pid, &status, 0);
	if (WIFEXITED(status))
		ppath = WEXITSTATUS(status);

	if (ppath == nonppath) {
		printf("[SUCCESS] ppath = %d, nonppath = %d\n", ppath, nonppath);
		return 0;
	}
	printf("[FAIL] ppath = %d, nonppath = %d\n", ppath, nonppath);
	return 1;
}

static int
test_open(int do_pp)
{
	char filename[256];
	int dirfd;
	int dirfd2;
	int dirfd3;


	PP_SHOULD_SUCCEED(((dirfd = open("/", O_RDONLY | O_DIRECTORY)) == -1), "open");
	PP_SHOULD_SUCCEED(((dirfd2 = open(pp_dir2, O_RDONLY | O_DIRECTORY)) == -1), "open");
	if (do_pp) {
		printf("testing open and openat\n");
		do_pledgepath();
	}
	PP_SHOULD_SUCCEED((pledge("stdio rpath cpath wpath", NULL) == -1), "pledge");

	PP_SHOULD_FAIL(((dirfd3= open(pp_dir2, O_RDONLY | O_DIRECTORY)) == -1), "open");

	PP_SHOULD_SUCCEED((open(pp_file1, O_RDWR) == -1), "open");
	if (!do_pp) {
		/* Unlink the pledgepathed file and make it again */
		PP_SHOULD_SUCCEED((unlink(pp_file1) == -1), "unlink");
		PP_SHOULD_SUCCEED((open(pp_file1, O_RDWR|O_CREAT) == -1), "open");
	}
	sleep(1);
	PP_SHOULD_SUCCEED((open(pp_file1, O_RDWR) == -1), "open");
	PP_SHOULD_SUCCEED((openat(dirfd, "etc/hosts", O_RDONLY) == -1), "openat");
	PP_SHOULD_FAIL((openat(dirfd, pp_file2, O_RDWR) == -1), "openat");
	PP_SHOULD_SUCCEED((openat(dirfd2, "hooray", O_RDWR|O_CREAT) == -1), "openat");
	PP_SHOULD_FAIL((open(pp_file2, O_RDWR) == -1), "open");
	(void) snprintf(filename, sizeof(filename), "%s/%s", pp_dir1, "newfile");
	PP_SHOULD_SUCCEED((open(filename, O_RDWR|O_CREAT) == -1), "open");
	(void) snprintf(filename, sizeof(filename), "%s/%s", pp_dir2, "newfile");
	PP_SHOULD_FAIL((open(filename, O_RDWR|O_CREAT) == -1), "open");

	return 0;
}

static int
test_unlink(int do_pp)
{
	char filename1[256];
	char filename2[256];
	char filename3[] = "/tmp/nukeme.XXXXXX";
	int fd;

	(void) snprintf(filename1, sizeof(filename1), "%s/%s", pp_dir1,
	    "nukeme");
	(void) snprintf(filename2, sizeof(filename2), "%s/%s", pp_dir2,
	    "nukeme");
	PP_SHOULD_SUCCEED((open(filename1, O_RDWR|O_CREAT) == -1), "open");
	PP_SHOULD_SUCCEED((open(filename2, O_RDWR|O_CREAT) == -1), "open");
	if ((fd = mkstemp(filename3)) == -1)
		err(1, "%s:%d - mkstemp", __FILE__, __LINE__);
	if (do_pp) {
		printf("testing unlink\n");
		do_pledgepath();
	}

	PP_SHOULD_SUCCEED((pledge("stdio rpath cpath wpath", NULL) == -1),
	    "pledge");
	PP_SHOULD_SUCCEED((unlink(filename1) == -1), "unlink");
	PP_SHOULD_FAIL((unlink(filename2) == -1), "unlink");
	PP_SHOULD_FAIL((unlink(filename3) == -1), "unlink");
	return 0;
}

static int
test_link(int do_pp)
{
	char filename[256];
	char filename2[256];

	if (do_pp) {
		printf("testing link\n");
		do_pledgepath();
	}

	PP_SHOULD_SUCCEED((pledge("stdio rpath cpath wpath", NULL) == -1),
	    "pledge");
	(void) snprintf(filename, sizeof(filename), "%s/%s", pp_dir1,
	    "linkpp1");
	(void) snprintf(filename2, sizeof(filename2), "%s/%s", pp_dir2,
	    "linkpp2");
	unlink(filename);
	unlink(filename2);
	PP_SHOULD_SUCCEED((link(pp_file1, filename) == -1), "link");
	unlink(filename);
	PP_SHOULD_FAIL((link(pp_file2, filename) == -1), "link");
	PP_SHOULD_FAIL((link(pp_file1, filename2) == -1), "link");

	return 0;
}


static int
test_chdir(int do_pp)
{
	if (do_pp) {
		printf("testing chdir\n");
		do_pledgepath();
	}

	PP_SHOULD_SUCCEED((pledge("stdio rpath", NULL) == -1), "pledge");
	PP_SHOULD_SUCCEED((chdir(pp_dir1) == -1), "chdir");
	PP_SHOULD_FAIL((chdir(pp_dir2) == -1), "chdir");

	return 0;
}

static int
test_rename(int do_pp)
{
	char filename1[256];
	char filename2[256];
	char rfilename1[256];
	char rfilename2[256];
	int dirfd1, dirfd2;

	if ((dirfd1 = open(pp_dir1, O_RDONLY | O_DIRECTORY)) == -1)
		err(1, "%s:%d - open of dir1", __FILE__, __LINE__);
	if ((dirfd2 = open(pp_dir2, O_RDONLY | O_DIRECTORY)) == -1)
		err(1, "%s:%d - open of dir2", __FILE__, __LINE__);
	(void) snprintf(filename1, sizeof(filename1), "%s/%s", pp_dir1,
	    "file1");
	PP_SHOULD_SUCCEED((open(filename1, O_RDWR|O_CREAT) == -1), "open");
	(void) snprintf(filename2, sizeof(filename2), "%s/%s", pp_dir2,
	    "file2");
        PP_SHOULD_SUCCEED((open(filename2, O_RDWR|O_CREAT) == -1), "open");
	(void) snprintf(rfilename1, sizeof(rfilename1), "%s/%s", pp_dir1,
	    "rfile1");
	(void) snprintf(rfilename2, sizeof(rfilename2), "%s/%s", pp_dir2,
	    "rfile2");
	if (do_pp) {
		printf("testing rename\n");
		do_pledgepath();
	}

	PP_SHOULD_SUCCEED((pledge("stdio rpath wpath cpath", NULL) == -1),
	    "pledge");
	PP_SHOULD_SUCCEED((rename(filename1, rfilename1) == -1), "rename");
	PP_SHOULD_FAIL((rename(filename2, rfilename2) == -1), "rename");
	PP_SHOULD_SUCCEED((open(filename1, O_RDWR|O_CREAT) == -1), "open");
	PP_SHOULD_FAIL((rename(filename1, rfilename2) == -1), "rename");
	PP_SHOULD_SUCCEED((open(filename1, O_RDWR|O_CREAT) == -1), "open");
	PP_SHOULD_FAIL((rename(filename1, pp_file2) == -1), "rename");
	PP_SHOULD_SUCCEED((open(filename1, O_RDWR|O_CREAT) == -1), "open");
	PP_SHOULD_SUCCEED((renameat(dirfd1, "file1", dirfd2, "rfile2") == -1),
	    "renameat");
	PP_SHOULD_SUCCEED((open(filename1, O_RDWR|O_CREAT) == -1), "open");
	PP_SHOULD_FAIL((renameat(dirfd1, "file1", dirfd2, rfilename2) == -1),
	    "renameat");

	return (0);
}


static int
test_access(int do_pp)
{
	if (do_pp) {
		printf("testing access\n");
		do_pledgepath();
	}

	PP_SHOULD_SUCCEED((pledge("stdio rpath", NULL) == -1), "pledge");
	PP_SHOULD_SUCCEED((access(pp_file1, R_OK) == -1), "access");
	PP_SHOULD_FAIL((access(pp_file2, R_OK) == -1), "access");
	PP_SHOULD_SUCCEED((access(pp_dir1, R_OK) == -1), "access");
	PP_SHOULD_FAIL((access(pp_dir2, R_OK) == -1), "access");

	return 0;
}

static int
test_chflags(int do_pp)
{
	if (do_pp) {
		printf("testing chflags\n");
		do_pledgepath();
	}

	PP_SHOULD_SUCCEED((pledge("stdio rpath", NULL) == -1), "pledge");
	PP_SHOULD_SUCCEED((chflags(pp_file1, UF_NODUMP) == -1), "chflags");
	PP_SHOULD_FAIL((chflags(pp_file2, UF_NODUMP) == -1), "chflags");

	return 0;
}

static int
test_stat(int do_pp)
{
	if (do_pp) {
		printf("testing stat\n");
		do_pledgepath();
	}
	struct stat sb;

	PP_SHOULD_SUCCEED((pledge("stdio rpath", NULL) == -1), "pledge");
	PP_SHOULD_SUCCEED((stat(pp_file1, &sb) == -1), "stat");
	PP_SHOULD_FAIL((stat(pp_file2, &sb) == -1), "stat");
	PP_SHOULD_SUCCEED((stat(pp_dir1, &sb) == -1), "stat");
	PP_SHOULD_FAIL((stat(pp_dir2, &sb) == -1), "stat");

	return 0;
}

static int
test_statfs(int do_pp)
{
	if (do_pp) {
		printf("testing statfs\n");
		do_pledgepath();
	}
	struct statfs sb;

	PP_SHOULD_SUCCEED((pledge("stdio rpath", NULL) == -1), "pledge");
	PP_SHOULD_SUCCEED((statfs(pp_file1, &sb) == -1), "statfs");
	PP_SHOULD_FAIL((statfs(pp_file2, &sb) == -1), "statfs");
	PP_SHOULD_SUCCEED((statfs(pp_dir1, &sb) == -1), "statfs");
	PP_SHOULD_FAIL((statfs(pp_dir2, &sb) == -1), "statfs");

	return 0;
}

static int
test_symlink(int do_pp)
{
	char filename[256];
	char filename2[256];
	char buf[256];
	struct stat sb;

	if (do_pp) {
		printf("testing symlink and lstat and readlink\n");
		do_pledgepath();
	}

	PP_SHOULD_SUCCEED((pledge("stdio rpath cpath wpath", NULL) == -1),
	    "pledge");
	(void) snprintf(filename, sizeof(filename), "%s/%s", pp_dir1,
	    "slinkpp1");
	(void) snprintf(filename2, sizeof(filename2), "%s/%s", pp_dir2,
	    "slinkpp2");
	unlink(filename);
	unlink(filename2);
	PP_SHOULD_SUCCEED((symlink(pp_file1, filename) == -1), "symlink");
	PP_SHOULD_SUCCEED((lstat(filename, &sb) == -1), "lstat");
	PP_SHOULD_SUCCEED((lstat(pp_file1, &sb) == -1), "lstat");
	PP_SHOULD_SUCCEED((readlink(filename, buf, sizeof(buf)) == -1), "readlink");
	unlink(filename);
	PP_SHOULD_SUCCEED((symlink(pp_file2, filename) == -1), "symlink");
	PP_SHOULD_SUCCEED((lstat(filename, &sb) == -1), "lstat");
	PP_SHOULD_SUCCEED((readlink(filename, buf, sizeof(buf)) == -1), "readlink");
	PP_SHOULD_FAIL((lstat(pp_file2, &sb) == -1), "lstat");
	PP_SHOULD_FAIL((symlink(pp_file1, filename2) == -1), "symlink");
	PP_SHOULD_FAIL((readlink(filename2, buf, sizeof(buf)) == -1), "readlink");

	return 0;
}

static int
test_chmod(int do_pp)
{
	if (do_pp) {
		printf("testing chmod\n");
		do_pledgepath();
	}

	PP_SHOULD_SUCCEED((pledge("stdio rpath wpath", NULL) == -1), "pledge");
	PP_SHOULD_SUCCEED((chmod(pp_file1, S_IRWXU) == -1), "chmod");
	PP_SHOULD_FAIL((chmod(pp_file1, S_IRWXU) == -1), "chmod");
	PP_SHOULD_SUCCEED((chmod(pp_dir1, S_IRWXU) == -1), "chmod");
	PP_SHOULD_FAIL((chmod(pp_dir2, S_IRWXU) == -1), "chmod");

	return 0;
}
static int
test_exec(int do_pp)
{
	if (do_pp) {
		printf("testing execve\n");
		do_pledgepath();
	}
	char *argv[] = {"/usr/bin/true", NULL};
	extern char **environ;

	PP_SHOULD_SUCCEED((pledge("stdio exec", NULL) == -1), "pledge");
	PP_SHOULD_SUCCEED((execve(argv[0], argv, environ) == -1), "execve");

	return 0;
}

int
main (int argc, char *argv[])
{
	int fd1, fd2, failures = 0;

	PP_SHOULD_SUCCEED((mkdtemp(pp_dir1) == NULL), "mkdtmp");
	PP_SHOULD_SUCCEED((mkdtemp(pp_dir2) == NULL), "mkdtmp");
	PP_SHOULD_SUCCEED(((fd1 = mkstemp(pp_file1)) == -1), "mkstemp");
	close(fd1);
	PP_SHOULD_SUCCEED(((fd2 = mkstemp(pp_file2)) == -1), "mkstemp");
	close(fd2);

	failures += runcompare(test_open);
	failures += runcompare(test_unlink);
	failures += runcompare(test_link);
	failures += runcompare(test_chdir);
	failures += runcompare(test_rename);
	failures += runcompare(test_access);
	failures += runcompare(test_chflags);
	failures += runcompare(test_stat);
	failures += runcompare(test_statfs);
	failures += runcompare(test_symlink);
	failures += runcompare(test_chmod);
	failures += runcompare(test_exec);

	exit(failures);
}
