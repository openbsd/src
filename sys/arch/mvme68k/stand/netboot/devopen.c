/*	$OpenBSD: devopen.c,v 1.2 2002/03/14 03:15:56 millert Exp $ */

#include <sys/param.h>
#include <stand.h>

/*
 * Open the device named by the combined device/file name
 * given as the "fname" arg, something like: "sd()bsd"
 *
 * However, Sun PROMs don't really let you choose which
 * device you will talk to.  You can only open the device
 * that was used to load the boot program.  Therefore, we
 * do not accept a "device" part in the "fname" string.
 * Pass the PROM device name to open in case it needs it.
 */
int
devopen(f, fname, file)
	struct open_file *f;
	const char *fname;
	char **file;
{
	struct devsw *dp;
	char *cp, *path, *devname;
	int error;

	*file = (char *)fname;
	dp = &devsw[0];
	f->f_dev = dp;
	error = (*dp->dv_open)(f, NULL);

	return (error);
}
