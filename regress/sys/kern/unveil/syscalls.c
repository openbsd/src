/*	$OpenBSD: syscalls.c,v 1.23 2019/05/15 21:05:55 beck Exp $	*/

/*
 * Copyright (c) 2017-2019 Bob Beck <beck@openbsd.org>
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
#include <dirent.h>
#include <err.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/errno.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/mount.h>

pid_t child;
char uv_dir1[] = "/tmp/uvdir1.XXXXXX"; /* unveiled */
char uv_dir2[] = "/tmp/uvdir2.XXXXXX"; /* not unveiled */
char uv_file1[] = "/tmp/uvfile1.XXXXXX"; /* unveiled */
char uv_file2[] = "/tmp/uvfile2.XXXXXX"; /* not unveiled */

#define UV_SHOULD_SUCCEED(A, B) do {						\
	if (A) {								\
		err(1, "%s:%d - %s", __FILE__, __LINE__, B);			\
	}									\
} while (0)

#define UV_SHOULD_ENOENT(A, B) do {						\
	if (A) {				 				\
		if (do_uv && errno != ENOENT)					\
			err(1, "%s:%d - %s", __FILE__, __LINE__, B);		\
	} else {								\
		if (do_uv)							\
			errx(1, "%s:%d - %s worked when it should not "		\
			    "have",  __FILE__, __LINE__, B);			\
	}									\
} while(0)

#define UV_SHOULD_EACCES(A, B) do {						\
	if (A) {				 				\
		if (do_uv && errno != EACCES)					\
			err(1, "%s:%d - %s", __FILE__, __LINE__, B);		\
	} else {								\
		if (do_uv)							\
			errx(1, "%s:%d - %s worked when it should not "		\
			    "have",  __FILE__, __LINE__, B);			\
	}									\
} while(0)

/* all the things unless we override */
const char *uv_flags = "rwxc";

static void
do_unveil(void)
{
	if (unveil(uv_dir1, uv_flags) == -1)
                err(1, "%s:%d - unveil", __FILE__, __LINE__);
	if (unveil(uv_file1, uv_flags) == -1)
                err(1, "%s:%d - unveil", __FILE__, __LINE__);
}

static void
do_unveil2(void)
{
	if (unveil(uv_dir1, uv_flags) == -1)
                err(1, "%s:%d - unveil", __FILE__, __LINE__);
}

static int
runcompare_internal(int (*func)(int), int fail_ok)
{
	int unveil = 0, nonunveil = 0, status;
	pid_t pid = fork();
	if (pid == 0) {
		exit(func(0));
	}
	status = 0;
	waitpid(pid, &status, 0);
	if (WIFEXITED(status))
		nonunveil = WEXITSTATUS(status);
	if (WIFSIGNALED(status)) {
		printf("[FAIL] nonunveil exited with signal %d\n", WTERMSIG(status));
		goto fail;
	}
	pid = fork();
	if (pid == 0) {
		exit(func(1));
	}
	status = 0;
	waitpid(pid, &status, 0);
	if (WIFEXITED(status))
		unveil = WEXITSTATUS(status);
	if (WIFSIGNALED(status)) {
		printf("[FAIL] nonunveil exited with signal %d\n", WTERMSIG(status));
		goto fail;
	}
	if (!fail_ok && (unveil || nonunveil)) {
		printf("[FAIL] unveil = %d, nonunveil = %d\n", unveil, nonunveil);
		goto fail;
	}
	if (unveil == nonunveil) {
		printf("[SUCCESS] unveil = %d, nonunveil = %d\n", unveil, nonunveil);
		return 0;
	}
	printf("[FAIL] unveil = %d, nonunveil = %d\n", unveil, nonunveil);
 fail:
	return 1;
}

static int
runcompare(int (*func)(int))
{
	return runcompare_internal(func, 1);
}

static int
test_open(int do_uv)
{
	char filename[256];
	int dirfd;
	int dirfd2;
	int dirfd3;


	UV_SHOULD_SUCCEED(((dirfd = open("/", O_RDONLY | O_DIRECTORY)) == -1), "open");
	UV_SHOULD_SUCCEED(((dirfd2 = open(uv_dir2, O_RDONLY | O_DIRECTORY)) == -1), "open");
	if (do_uv) {
		printf("testing open and openat\n");
		do_unveil();
		if (unveil("/tmp/alpha", uv_flags) == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/tmp/bravo", uv_flags) == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/tmp/charlie", uv_flags) == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/tmp/delta", uv_flags) == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/tmp/echo", uv_flags) == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/tmp/foxtrot", uv_flags) == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/tmp/golf", uv_flags) == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/tmp/hotel", uv_flags) == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/tmp/india", uv_flags) == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/tmp/juliet", uv_flags) == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/tmp/kilo", uv_flags) == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/tmp/lima", uv_flags) == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/tmp/money", uv_flags) == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/tmp/november", uv_flags) == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/tmp/oscar", uv_flags) == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/tmp/papa", uv_flags) == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/tmp/quebec", uv_flags) == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/tmp/romeo", uv_flags) == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/tmp/sierra", uv_flags) == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/tmp/tango", uv_flags) == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/tmp/uniform", uv_flags) == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/tmp/victor", uv_flags) == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/tmp/whiskey", uv_flags) == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/tmp/xray", uv_flags) == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/tmp/yankee", uv_flags) == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/tmp/zulu", uv_flags) == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
	}
	UV_SHOULD_SUCCEED((pledge("unveil stdio rpath cpath wpath exec", NULL) == -1), "pledge");

	UV_SHOULD_ENOENT((open(uv_file2, O_RDWR) == -1), "open");
	UV_SHOULD_ENOENT(((dirfd3= open(uv_dir2, O_RDONLY | O_DIRECTORY)) == -1), "open");

	UV_SHOULD_SUCCEED((open(uv_file1, O_RDWR) == -1), "open");
	if (!do_uv) {
		/* Unlink the unveiled file and make it again */
		UV_SHOULD_SUCCEED((unlink(uv_file1) == -1), "unlink");
		UV_SHOULD_SUCCEED((open(uv_file1, O_RDWR|O_CREAT) == -1), "open");
	}
	sleep(1);
	UV_SHOULD_SUCCEED((open(uv_file1, O_RDWR) == -1), "open");
	UV_SHOULD_ENOENT((openat(dirfd, "etc/hosts", O_RDONLY) == -1), "openat");
	UV_SHOULD_ENOENT((openat(dirfd, uv_file2, O_RDWR) == -1), "openat");
	UV_SHOULD_ENOENT((openat(dirfd2, "hooray", O_RDWR|O_CREAT) == -1), "openat");
	UV_SHOULD_ENOENT((open(uv_file2, O_RDWR) == -1), "open");
	(void) snprintf(filename, sizeof(filename), "%s/%s", uv_dir1, "newfile");
	UV_SHOULD_SUCCEED((open(filename, O_RDWR|O_CREAT) == -1), "open");
	(void) snprintf(filename, sizeof(filename), "/%s/%s", uv_dir1, "doubleslash");
	UV_SHOULD_SUCCEED((open(filename, O_RDWR|O_CREAT) == -1), "open");
	(void) snprintf(filename, sizeof(filename), "/%s//%s", uv_dir1, "doubleslash2");
	UV_SHOULD_SUCCEED((open(filename, O_RDWR|O_CREAT) == -1), "open");

	(void) snprintf(filename, sizeof(filename), "%s/%s", uv_dir2, "newfile");
	UV_SHOULD_ENOENT((open(filename, O_RDWR|O_CREAT) == -1), "open");

	if (do_uv) {
		printf("testing flag escalation\n");
		if (unveil(uv_file1, "x") == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil(uv_file1, "rx") == -1)
			if (errno != EPERM)
				err(1, "%s:%d - unveil", __FILE__,
				    __LINE__);
	}
	return 0;
}

static int
test_opendir(int do_uv)
{
	char filename[256];
	if (do_uv) {
		printf("testing opendir\n");
		do_unveil();
	}
	UV_SHOULD_SUCCEED((opendir(uv_dir1) == NULL), "opendir");
	UV_SHOULD_ENOENT((opendir(uv_dir2) == NULL), "opendir");
	(void) snprintf(filename, sizeof(filename), "/%s/.", uv_dir1);
	UV_SHOULD_SUCCEED((opendir(filename) == NULL), "opendir");
	(void) snprintf(filename, sizeof(filename), "/%s/..", uv_dir1);
	UV_SHOULD_ENOENT((opendir(filename) == NULL), "opendir");
	(void) snprintf(filename, sizeof(filename), "/%s/subdir", uv_dir1);
	UV_SHOULD_SUCCEED((opendir(filename) == NULL), "opendir");
	(void) snprintf(filename, sizeof(filename), "/%s/subdir/../subdir", uv_dir1);
	UV_SHOULD_SUCCEED((opendir(filename) == NULL), "opendir");
	(void) snprintf(filename, sizeof(filename), "/%s/../../%s/subdir", uv_dir1, uv_dir1);
	UV_SHOULD_SUCCEED((opendir(filename) == NULL), "opendir");
	(void) snprintf(filename, sizeof(filename), "/%s/subdir", uv_dir2);
	UV_SHOULD_ENOENT((opendir(filename) == NULL), "opendir");
	UV_SHOULD_ENOENT((opendir(filename) == NULL), "opendir");
	(void) snprintf(filename, sizeof(filename), "%s/../..%s/subdir", uv_dir1, uv_dir2);
	UV_SHOULD_ENOENT((opendir(filename) == NULL), "opendir");
	return 0;
}

static int
test_realpath(int do_uv)
{
	char buf[PATH_MAX];
	if (do_uv) {
		printf("testing realpath\n");
		do_unveil();
	}
	UV_SHOULD_SUCCEED((realpath(uv_dir1, buf) == NULL), "realpath");
	return 0;
	UV_SHOULD_ENOENT((realpath(uv_dir2, buf) == NULL), "realpath");
	return 0;
}

static int
test_r(int do_uv)
{
	if (do_uv) {
		printf("testing \"r\"\n");
		if (unveil(uv_file1, "r") == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/", "") == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
	}
	UV_SHOULD_SUCCEED((open(uv_file1, O_RDONLY) == -1), "open");
	UV_SHOULD_EACCES((open(uv_file1, O_RDWR) == -1), "open");
	return 0;
}

static int
test_rw(int do_uv)
{
	if (do_uv) {
		printf("testing \"rw\"\n");
		if (unveil(uv_file1, "rw") == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/", "") == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
	}
	UV_SHOULD_SUCCEED((open(uv_file1, O_RDWR) == -1), "open");
	UV_SHOULD_SUCCEED((open(uv_file1, O_RDONLY) == -1), "open");
	return 0;
}

static int
test_x(int do_uv)
{
	struct stat sb;
	if (do_uv) {
		printf("testing \"x\"\n");
		if (unveil(uv_file1, "x") == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/", "") == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
	}
	UV_SHOULD_EACCES((lstat(uv_file1, &sb) == -1), "lstat");
	UV_SHOULD_EACCES((open(uv_file1, O_RDONLY) == -1), "open");
	UV_SHOULD_EACCES((open(uv_file1, O_RDONLY) == -1), "open");
	UV_SHOULD_ENOENT((open(uv_file2, O_RDWR) == -1), "open");
	return 0;
}

static int
test_noflags(int do_uv)
{
	char filename[256];

	if (do_uv) {
		printf("testing clearing flags\n");
		do_unveil();
	}

	UV_SHOULD_SUCCEED((pledge("unveil stdio rpath cpath wpath exec", NULL) == -1), "pledge");
	UV_SHOULD_SUCCEED((open(uv_file1, O_RDWR) == -1), "open");
	if (do_uv) {
		if (unveil(uv_dir1, "") == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
	}
	(void) snprintf(filename, sizeof(filename), "%s/%s", uv_dir1, "noflagsiamboned");
	UV_SHOULD_ENOENT((open(filename, O_RDWR|O_CREAT) == -1), "open");
	UV_SHOULD_SUCCEED((open(uv_file1, O_RDWR) == -1), "open");
	return 0;
}


static int
test_drounveil(int do_uv)
{
	if (do_uv) {
		printf("(testing unveil after pledge)\n");
		do_unveil();
	}

	UV_SHOULD_SUCCEED((pledge("unveil stdio rpath cpath wpath exec", NULL) == -1), "pledge");

	if (do_uv) {
		do_unveil();
	}
	UV_SHOULD_SUCCEED((pledge("stdio rpath cpath wpath", NULL) == -1), "pledge");

	UV_SHOULD_ENOENT((open(uv_file2, O_RDWR) == -1), "open");
	UV_SHOULD_SUCCEED((open(uv_file1, O_RDWR) == -1), "open");
	return 0;
}

static int
test_unlink(int do_uv)
{
	char filename1[256];
	char filename2[256];
	char filename3[] = "/tmp/nukeme.XXXXXX";
	int fd;

	(void) snprintf(filename1, sizeof(filename1), "%s/%s", uv_dir1,
	    "nukeme");
	(void) snprintf(filename2, sizeof(filename2), "%s/%s", uv_dir2,
	    "nukeme");
	UV_SHOULD_SUCCEED((open(filename1, O_RDWR|O_CREAT) == -1), "open");
	UV_SHOULD_SUCCEED((open(filename2, O_RDWR|O_CREAT) == -1), "open");
	if ((fd = mkstemp(filename3)) == -1)
		err(1, "%s:%d - mkstemp", __FILE__, __LINE__);
	if (do_uv) {
		printf("testing unlink\n");
		do_unveil();
		if (unveil(filename3, "rw") == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
	}

	UV_SHOULD_SUCCEED((pledge("unveil stdio fattr rpath cpath wpath", NULL) == -1),
	    "pledge");
	UV_SHOULD_SUCCEED((unlink(filename1) == -1), "unlink");
	UV_SHOULD_ENOENT((unlink(filename2) == -1), "unlink");
	UV_SHOULD_EACCES((unlink(filename3) == -1), "unlink");
	return 0;
}

static int
test_link(int do_uv)
{
	char filename[256];
	char filename2[256];

	if (do_uv) {
		printf("testing link\n");
		do_unveil();
	}

	UV_SHOULD_SUCCEED((pledge("unveil stdio fattr rpath cpath wpath", NULL) == -1),
	    "pledge");
	(void) snprintf(filename, sizeof(filename), "%s/%s", uv_dir1,
	    "linkuv1");
	(void) snprintf(filename2, sizeof(filename2), "%s/%s", uv_dir2,
	    "linkuv2");
	unlink(filename);
	unlink(filename2);
	UV_SHOULD_SUCCEED((link(uv_file1, filename) == -1), "link");
	unlink(filename);
	UV_SHOULD_ENOENT((link(uv_file2, filename) == -1), "link");
	UV_SHOULD_ENOENT((link(uv_file1, filename2) == -1), "link");
	if (do_uv) {
		printf("testing link without O_CREAT\n");
		if (unveil(filename, "rw") == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);

	}
	UV_SHOULD_EACCES((link(uv_file1, filename) == -1), "link");
	unlink(filename);

	return 0;
}


static int
test_chdir(int do_uv)
{
	if (do_uv) {
		printf("testing chdir\n");
		do_unveil2();
	}

	UV_SHOULD_SUCCEED((pledge("stdio fattr rpath", NULL) == -1), "pledge");
	UV_SHOULD_ENOENT((chdir(uv_dir2) == -1), "chdir");
	UV_SHOULD_SUCCEED((chdir(uv_dir1) == -1), "chdir");
	UV_SHOULD_ENOENT((chdir(uv_dir2) == -1), "chdir");

	return 0;
}

static int
test_parent_dir(int do_uv)
{
	char filename[255];
	if (do_uv) {
		printf("testing parent dir\n");
		do_unveil2();
	} else {
		(void) snprintf(filename, sizeof(filename), "/%s/doof", uv_dir1);
		UV_SHOULD_SUCCEED((mkdir(filename, 0777) == -1), "mkdir");
		(void) snprintf(filename, sizeof(filename), "/%s/doof/subdir2", uv_dir1);
		UV_SHOULD_SUCCEED((mkdir(filename, 0777) == -1), "mkdir");
		(void) snprintf(filename, sizeof(filename), "/%s/doof/subdir1", uv_dir1);
		UV_SHOULD_SUCCEED((mkdir(filename, 0777) == -1), "mkdir");
		(void) snprintf(filename, sizeof(filename), "/%s/doof/subdir1/poop", uv_dir1);
		UV_SHOULD_SUCCEED((open(filename, O_RDWR|O_CREAT, 0644) == -1), "open");
		(void) snprintf(filename, sizeof(filename), "/%s/doof/subdir1/link", uv_dir1);
		UV_SHOULD_SUCCEED((symlink("../subdir1/poop", filename) == -1), "symlink");
	}
	sleep(1);
	(void) snprintf(filename, sizeof(filename), "/%s/doof/subdir1/link", uv_dir1);
	UV_SHOULD_SUCCEED((access(filename, R_OK) == -1), "access");
	(void) snprintf(filename, sizeof(filename), "/%s/doof/subdir1/poop", uv_dir1);
	UV_SHOULD_SUCCEED((access(filename, R_OK) == -1), "access");
	UV_SHOULD_SUCCEED((chdir(uv_dir1) == -1), "chdir");
	(void) snprintf(filename, sizeof(filename), "/%s/doof/subdir1", uv_dir1);
	UV_SHOULD_SUCCEED((chdir(filename) == -1), "chdir");
	UV_SHOULD_SUCCEED((access("poop", R_OK) == -1), "access");
	UV_SHOULD_SUCCEED((chdir("../subdir2") == -1), "chdir");
	UV_SHOULD_SUCCEED((chdir("../subdir1") == -1), "chdir");
	UV_SHOULD_SUCCEED((chdir(filename) == -1), "chdir");
	UV_SHOULD_SUCCEED((chdir("../../doof/subdir2") == -1), "chdir");
	UV_SHOULD_SUCCEED((chdir("../../doof/subdir1") == -1), "chdir");
	UV_SHOULD_SUCCEED((chdir("../../doof/subdir2") == -1), "chdir");
	UV_SHOULD_SUCCEED((chdir("../../doof/subdir1") == -1), "chdir");
	UV_SHOULD_SUCCEED((access("poop", R_OK) == -1), "access");
	UV_SHOULD_SUCCEED((access("../subdir1/poop", R_OK) == -1), "access");
	UV_SHOULD_ENOENT((chdir("../../../") == -1), "chdir");
	UV_SHOULD_ENOENT((chdir(uv_dir2) == -1), "chdir");
	return(0);
}

static int
test_rename(int do_uv)
{
	char filename1[256];
	char filename2[256];
	char rfilename1[256];
	char rfilename2[256];
	int dirfd1, dirfd2;

	if ((dirfd1 = open(uv_dir1, O_RDONLY | O_DIRECTORY)) == -1)
		err(1, "%s:%d - open of dir1", __FILE__, __LINE__);
	if ((dirfd2 = open(uv_dir2, O_RDONLY | O_DIRECTORY)) == -1)
		err(1, "%s:%d - open of dir2", __FILE__, __LINE__);
	(void) snprintf(filename1, sizeof(filename1), "%s/%s", uv_dir1,
	    "file1");
	UV_SHOULD_SUCCEED((open(filename1, O_RDWR|O_CREAT) == -1), "open");
	(void) snprintf(filename2, sizeof(filename2), "%s/%s", uv_dir2,
	    "file2");
        UV_SHOULD_SUCCEED((open(filename2, O_RDWR|O_CREAT) == -1), "open");
	(void) snprintf(rfilename1, sizeof(rfilename1), "%s/%s", uv_dir1,
	    "rfile1");
	(void) snprintf(rfilename2, sizeof(rfilename2), "%s/%s", uv_dir2,
	    "rfile2");
	if (do_uv) {
		printf("testing rename\n");
		do_unveil();
	}

	UV_SHOULD_SUCCEED((pledge("stdio fattr rpath wpath cpath", NULL) == -1),
	    "pledge");
	UV_SHOULD_SUCCEED((rename(filename1, rfilename1) == -1), "rename");
	UV_SHOULD_ENOENT((rename(filename2, rfilename2) == -1), "rename");
	UV_SHOULD_SUCCEED((open(filename1, O_RDWR|O_CREAT) == -1), "open");
	UV_SHOULD_ENOENT((rename(filename1, rfilename2) == -1), "rename");
	UV_SHOULD_SUCCEED((open(filename1, O_RDWR|O_CREAT) == -1), "open");
	UV_SHOULD_ENOENT((rename(filename1, uv_file2) == -1), "rename");
	UV_SHOULD_SUCCEED((open(filename1, O_RDWR|O_CREAT) == -1), "open");
	UV_SHOULD_ENOENT((renameat(dirfd1, "file1", dirfd2, "rfile2") == -1),
	    "renameat");
	UV_SHOULD_SUCCEED((open(filename1, O_RDWR|O_CREAT) == -1), "open");
	UV_SHOULD_ENOENT((renameat(dirfd1, "file1", dirfd2, rfilename2) == -1),
	    "renameat");

	return (0);
}


static int
test_access(int do_uv)
{
	if (do_uv) {
		printf("testing access\n");
		do_unveil();
	}

	UV_SHOULD_SUCCEED((access(uv_file1, R_OK) == -1), "access");
	UV_SHOULD_ENOENT((access(uv_file2, R_OK) == -1), "access");
	UV_SHOULD_ENOENT((access("/etc/passwd", R_OK) == -1), "access");
	UV_SHOULD_SUCCEED((access(uv_dir1, R_OK) == -1), "access");
	UV_SHOULD_ENOENT((access(uv_dir2, R_OK) == -1), "access");
	UV_SHOULD_ENOENT((access("/", R_OK) == -1), "access");
	UV_SHOULD_ENOENT((access("/home", F_OK) == -1), "access");

	UV_SHOULD_SUCCEED((pledge("stdio fattr rpath", NULL) == -1), "pledge");
	UV_SHOULD_SUCCEED((access(uv_file1, R_OK) == -1), "access");
	UV_SHOULD_ENOENT((access(uv_file2, R_OK) == -1), "access");
	UV_SHOULD_SUCCEED((access(uv_dir1, R_OK) == -1), "access");
	UV_SHOULD_ENOENT((access(uv_dir2, R_OK) == -1), "access");
	UV_SHOULD_ENOENT((access("/", R_OK) == -1), "access");
	UV_SHOULD_ENOENT((access("/home", F_OK) == -1), "access");

	return 0;
}

static int
test_chflags(int do_uv)
{
	if (do_uv) {
		printf("testing chflags\n");
		do_unveil();
	}

	UV_SHOULD_SUCCEED((pledge("stdio fattr rpath", NULL) == -1), "pledge");
	UV_SHOULD_SUCCEED((chflags(uv_file1, UF_NODUMP) == -1), "chflags");
	UV_SHOULD_ENOENT((chflags(uv_file2, UF_NODUMP) == -1), "chflags");

	return 0;
}

static int
test_stat(int do_uv)
{
	if (do_uv) {
		printf("testing stat\n");
		do_unveil();
	}
	struct stat sb;

	UV_SHOULD_SUCCEED((pledge("stdio fattr rpath", NULL) == -1), "pledge");
	UV_SHOULD_SUCCEED((stat(uv_file1, &sb) == -1), "stat");
	UV_SHOULD_ENOENT((stat(uv_file2, &sb) == -1), "stat");
	UV_SHOULD_SUCCEED((stat(uv_dir1, &sb) == -1), "stat");
	UV_SHOULD_ENOENT((stat(uv_dir2, &sb) == -1), "stat");
	UV_SHOULD_ENOENT((stat("/", &sb) == -1), "stat");

	return 0;
}

static int
test_stat2(int do_uv)
{
	if (do_uv) {
		printf("testing stat components to nonexistent \"rw\"\n");
		if (unveil("/usr/share/man/nonexistent", "rw") == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
	}
	struct stat sb;

	UV_SHOULD_SUCCEED((pledge("stdio fattr rpath", NULL) == -1), "pledge");
	UV_SHOULD_ENOENT((stat("/", &sb) == -1), "stat");
	UV_SHOULD_ENOENT((stat("/usr", &sb) == -1), "stat");
	UV_SHOULD_ENOENT((stat("/usr/share", &sb) == -1), "stat");
	UV_SHOULD_ENOENT((stat("/usr/share/man", &sb) == -1), "stat");
	UV_SHOULD_ENOENT((stat("/usr/share/man/nonexistent", &sb) == -1), "stat");
	return 0;
}

static int
test_statfs(int do_uv)
{
	if (do_uv) {
		printf("testing statfs\n");
		do_unveil();
	}
	struct statfs sb;


	UV_SHOULD_SUCCEED((statfs("/home", &sb) == -1), "statfs");
	UV_SHOULD_SUCCEED((statfs("/", &sb) == -1), "statfs");
	UV_SHOULD_SUCCEED((statfs(uv_file1, &sb) == -1), "statfs");
	UV_SHOULD_SUCCEED((statfs(uv_file2, &sb) == -1), "statfs");
	UV_SHOULD_SUCCEED((statfs(uv_dir1, &sb) == -1), "statfs");
	UV_SHOULD_SUCCEED((statfs(uv_dir2, &sb) == -1), "statfs");
	UV_SHOULD_SUCCEED((pledge("stdio fattr rpath", NULL) == -1), "pledge");
	UV_SHOULD_SUCCEED((statfs(uv_file1, &sb) == -1), "statfs");
	UV_SHOULD_SUCCEED((statfs(uv_file2, &sb) == -1), "statfs");
	UV_SHOULD_SUCCEED((statfs(uv_dir1, &sb) == -1), "statfs");
	UV_SHOULD_SUCCEED((statfs(uv_dir2, &sb) == -1), "statfs");

	return 0;
}

static int
test_symlink(int do_uv)
{
	char filename[256];
	char filename2[256];
	char buf[256];
	struct stat sb;

	if (do_uv) {
		printf("testing symlink and lstat and readlink\n");
		do_unveil();
	}

	UV_SHOULD_SUCCEED((pledge("unveil stdio fattr rpath cpath wpath", NULL) == -1),
	    "pledge");
	(void) snprintf(filename, sizeof(filename), "%s/%s", uv_dir1,
	    "slinkuv1");
	(void) snprintf(filename2, sizeof(filename2), "%s/%s", uv_dir2,
	    "slinkuv2");
	unlink(filename);
	unlink(filename2);
	UV_SHOULD_SUCCEED((symlink(uv_file1, filename) == -1), "symlink");
	UV_SHOULD_SUCCEED((lstat(filename, &sb) == -1), "lstat");
	UV_SHOULD_SUCCEED((lstat(uv_file1, &sb) == -1), "lstat");
	UV_SHOULD_SUCCEED((readlink(filename, buf, sizeof(buf)) == -1), "readlink");
	unlink(filename);
	UV_SHOULD_SUCCEED((symlink(uv_file2, filename) == -1), "symlink");
	UV_SHOULD_SUCCEED((lstat(filename, &sb) == -1), "lstat");
	UV_SHOULD_SUCCEED((readlink(filename, buf, sizeof(buf)) == -1), "readlink");
	UV_SHOULD_ENOENT((lstat(uv_file2, &sb) == -1), "lstat");
	UV_SHOULD_ENOENT((symlink(uv_file1, filename2) == -1), "symlink");
	UV_SHOULD_ENOENT((readlink(filename2, buf, sizeof(buf)) == -1), "readlink");
	unlink(filename);

	if (do_uv) {
		printf("testing symlink with \"rw\"\n");
		if (unveil(filename, "rw") == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
	}
	UV_SHOULD_EACCES((symlink(uv_file1, filename) == -1), "symlink");

	return 0;
}

static int
test_chmod(int do_uv)
{
	if (do_uv) {
		printf("testing chmod\n");
		do_unveil();
	}
	UV_SHOULD_SUCCEED((pledge("stdio fattr rpath unveil", NULL) == -1), "pledge");
	UV_SHOULD_SUCCEED((chmod(uv_file1, S_IRWXU) == -1), "chmod");
	UV_SHOULD_ENOENT((chmod(uv_file2, S_IRWXU) == -1), "chmod");
	UV_SHOULD_SUCCEED((chmod(uv_dir1, S_IRWXU) == -1), "chmod");
	UV_SHOULD_ENOENT((chmod(uv_dir2, S_IRWXU) == -1), "chmod");
	if (do_uv) {
		printf("testing chmod should fail for read\n");
		if (unveil(uv_file1, "r") == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
	}
	UV_SHOULD_EACCES((chmod(uv_file1, S_IRWXU) == -1), "chmod");
	return 0;
}


static int
test_fork_body(int do_uv)
{
	UV_SHOULD_SUCCEED((open(uv_file1, O_RDWR|O_CREAT) == -1), "open after fork");
	UV_SHOULD_SUCCEED((opendir(uv_dir1) == NULL), "opendir after fork");
	UV_SHOULD_ENOENT((opendir(uv_dir2) == NULL), "opendir after fork");
	UV_SHOULD_ENOENT((open(uv_file2, O_RDWR|O_CREAT) == -1), "open after fork");
	return 0;
}
static int
test_fork()
{
	printf("testing fork inhertiance\n");
	do_unveil();
	return runcompare_internal(test_fork_body, 0);
}

static int
test_exec(int do_uv)
{
	char *argv[] = {"/usr/bin/true", NULL};
	extern char **environ;
	if (do_uv) {
		printf("testing execve with \"x\"\n");
		if (unveil("/usr/bin/true", "x") == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		/* dynamic linking requires this */
		if (unveil("/usr/lib", "r") == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/usr/libexec/ld.so", "r") == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
	}
	UV_SHOULD_SUCCEED((pledge("unveil stdio fattr exec", NULL) == -1), "pledge");
	UV_SHOULD_SUCCEED((execve(argv[0], argv, environ) == -1), "execve");
	return 0;
}

static int
test_exec2(int do_uv)
{
	char *argv[] = {"/usr/bin/true", NULL};
	extern char **environ;
	if (do_uv) {
		printf("testing execve with \"rw\"\n");
		if (unveil("/usr/bin/true", "rw") == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		/* dynamic linking requires this */
		if (unveil("/usr/lib", "r") == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/usr/libexec/ld.so", "r") == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
	}
	UV_SHOULD_SUCCEED((pledge("unveil stdio fattr exec", NULL) == -1), "pledge");
	UV_SHOULD_EACCES((execve(argv[0], argv, environ) == -1), "execve");
	return 0;
}

static int
test_slash(int do_uv)
{
	extern char **environ;
	if (do_uv) {
		printf("testing unveil(\"/\")\n");
		if (unveil("/bin/sh", "x") == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/", "r") == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
	}
	return 0;
}

static int
test_dot(int do_uv)
{
	extern char **environ;
	if (do_uv) {
		printf("testing dot(\".\")\n");
		if (unveil(".", "rwxc") == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if ((unlink(".") == -1) && errno != EPERM)
			err(1, "%s:%d - unlink", __FILE__, __LINE__);
		printf("testing dot flags(\".\")\n");
		if (unveil(".", "r") == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if ((unlink(".") == -1) && errno != EACCES)
			warn("%s:%d - unlink", __FILE__, __LINE__);
	}
	return 0;
}

static int
test_bypassunveil(int do_uv)
{
	if (do_uv) {
		printf("testing BYPASSUNVEIL\n");
		do_unveil2();
	}
	char filename3[] = "/tmp/nukeme.XXXXXX";

	UV_SHOULD_SUCCEED((pledge("stdio tmppath", NULL) == -1), "pledge");
	UV_SHOULD_SUCCEED((mkstemp(filename3) == -1), "mkstemp");

	return 0;
}


static int
test_dotdotup(int do_uv)
{
	UV_SHOULD_SUCCEED((open("/tmp/hello", O_RDWR|O_CREAT) == -1), "open");
	if (do_uv) {
		printf("testing dotdotup\n");
		do_unveil2();
	}
	if ((chdir(uv_dir1) == -1)) {
		err(1, "chdir");
	}
	UV_SHOULD_SUCCEED((open("./derp", O_RDWR|O_CREAT) == -1), "open");
	UV_SHOULD_SUCCEED((open("derp", O_RDWR|O_CREAT) == -1), "open");
	UV_SHOULD_ENOENT((open("../hello", O_RDWR|O_CREAT) == -1), "open");
	UV_SHOULD_ENOENT((open(".././hello", O_RDWR|O_CREAT) == -1), "open");
	return 0;
}

static int
test_kn(int do_uv)
{
	if (do_uv) {
		printf("testing read only with one writeable file\n");
		if (unveil("/", "r") == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
		if (unveil("/dev/null", "rw") == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
	}
	UV_SHOULD_SUCCEED((open("/dev/null", O_RDWR) == -1), "open");
	UV_SHOULD_SUCCEED((open("/dev/zero", O_RDONLY) == -1), "open");
	UV_SHOULD_EACCES((open("/dev/zero", O_RDWR) == -1), "open");
	return 0;
}


static int
test_pathdiscover(int do_uv)
{
	struct stat sb;
	if (do_uv) {
		printf("testing path discovery\n");
		if (unveil("/usr/share/man", "rx") == -1)
			err(1, "%s:%d - unveil", __FILE__, __LINE__);
	}
	UV_SHOULD_SUCCEED((lstat("/usr/share/man", &sb) == -1), "lstat");
	UV_SHOULD_SUCCEED((lstat("/usr/share/man/../../share/man", &sb) == -1), "lstat");
	/* XXX XXX XXX This should fail */
	UV_SHOULD_SUCCEED((lstat("/usr/share/man/../../local/../share/man", &sb) == -1), "lstat");
	return 0;
}


int
main (int argc, char *argv[])
{
	int fd1, fd2, failures = 0;
	char filename[256];

	UV_SHOULD_SUCCEED((mkdtemp(uv_dir1) == NULL), "mkdtmp");
	UV_SHOULD_SUCCEED((mkdtemp(uv_dir2) == NULL), "mkdtmp");
	UV_SHOULD_SUCCEED(((fd1 = mkstemp(uv_file1)) == -1), "mkstemp");
	close(fd1);
	UV_SHOULD_SUCCEED((chmod(uv_file1, S_IRWXU) == -1), "chmod");
	UV_SHOULD_SUCCEED(((fd2 = mkstemp(uv_file2)) == -1), "mkstemp");
	(void) snprintf(filename, sizeof(filename), "/%s/subdir", uv_dir1);
	UV_SHOULD_SUCCEED((mkdir(filename, 0777) == -1), "mkdir");
	(void) snprintf(filename, sizeof(filename), "/%s/subdir", uv_dir2);
	UV_SHOULD_SUCCEED((mkdir(filename, 0777) == -1), "mkdir");
	close(fd2);

	failures += runcompare(test_open);
	failures += runcompare(test_opendir);
	failures += runcompare(test_noflags);
	failures += runcompare(test_drounveil);
	failures += runcompare(test_r);
	failures += runcompare(test_rw);
	failures += runcompare(test_x);
	failures += runcompare(test_unlink);
	failures += runcompare(test_link);
	failures += runcompare(test_chdir);
	failures += runcompare(test_rename);
	failures += runcompare(test_access);
	failures += runcompare(test_chflags);
	failures += runcompare(test_stat);
	failures += runcompare(test_stat2);
	failures += runcompare(test_statfs);
	failures += runcompare(test_symlink);
	failures += runcompare(test_chmod);
	failures += runcompare(test_exec);
	failures += runcompare(test_exec2);
	failures += runcompare(test_realpath);
	failures += runcompare(test_parent_dir);
	failures += runcompare(test_slash);
	failures += runcompare(test_dot);
	failures += runcompare(test_bypassunveil);
	failures += runcompare_internal(test_fork, 0);
	failures += runcompare(test_dotdotup);
	failures += runcompare(test_kn);
	failures += runcompare(test_pathdiscover);
	exit(failures);
}
