
#include <sys/param.h>
#include <stand.h>

/*
 * Open the device 
 */
int
devopen(f, fname, file)
	struct open_file *f;
	const char *fname;
	char **file;
{
	struct devsw *dp;
	int error;

	*file = (char*)fname;
	dp = &devsw[0];
	f->f_dev = dp;
	error = (*dp->dv_open)(f, NULL);

	return (error);
}
