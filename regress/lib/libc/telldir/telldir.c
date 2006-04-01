/*	$OpenBSD: telldir.c,v 1.2 2006/04/01 18:24:53 otto Exp $	*/

/*	Written by Otto Moerbeek, 2006,  Public domain.	*/

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <err.h>
#include <limits.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


#define NFILES 1000

void
createfiles(void)
{
	int i, fd;
	char file[PATH_MAX];

	mkdir("d", 0755);
	for (i = 0; i < NFILES; i++) {
		snprintf(file, sizeof file, "d/%d", i);
		if ((fd = open(file, O_CREAT | O_WRONLY, 0600)) == -1)
			err(1, "open %s", file);
		close(fd);
	}
}

void
delfiles(void)
{
	DIR *dp;
	struct dirent *f;
	char file[PATH_MAX];

	dp = opendir("d");
	if (dp == NULL)
		err(1, "opendir");
	while (f = readdir(dp)) {
		if (strcmp(f->d_name, ".") == 0 ||
		    strcmp(f->d_name, "..") == 0)
			continue;
		snprintf(file, sizeof file, "d/%s", f->d_name);
		if (unlink(file) == -1)
			err(1, "unlink %s", f->d_name);
	}
	closedir(dp);
	if (rmdir("d") == -1)
		err(1, "rmdir");
}

void
loop(DIR *dp, int i)
{
	struct dirent *f;
	char file[PATH_MAX];
	long pos, remember = -1;

	rewinddir(dp);
	snprintf(file, sizeof file, "%d", i);
	for (;;) {
		pos = telldir(dp);
		f = readdir(dp);
		if (f == NULL)
			break;
		if (strcmp(file, f->d_name) == 0)
			remember = pos;
	}
	if (remember == -1)
		errx(1, "remember");
	seekdir(dp, remember);
	if (telldir(dp) != remember)
		errx(1, "tell after seek");
	if (telldir(dp) != remember)
		errx(1, "tell after tell");
	f = readdir(dp);
	if (f == NULL)
		err(1, "seek to %s %ld", file, remember);

	if (strcmp(f->d_name, file) != 0)
		err(1, "name mismatch: %s != %s\n", f->d_name, file);
}

int
main(void)
{
	DIR *dp;
	int i;

	createfiles();

	dp = opendir("d");
	if (dp == NULL)
		err(1, "opendir");

	for (i = 0; i < NFILES; i++)
		loop(dp, (i + NFILES/2) % NFILES);

	closedir(dp);
	delfiles();
	return 0;
}
