#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <unistd.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
	struct iovec iov[2];
	int mib[2];
	size_t len;
	char *p;

	mib[0] = CTL_HW;
	mib[1] = HW_DISKNAMES;

	if (sysctl(mib, 2, NULL, &len, NULL, 0) != -1)
		if ((p = malloc(len)) != NULL)
			if (sysctl(mib, 2, p, &len, NULL, 0) != -1) {
				iov[0].iov_base = p;
				iov[0].iov_len = len;
				iov[1].iov_base = "\n";
				iov[1].iov_len = 1;
				writev(STDOUT_FILENO, iov, 2);
				exit(0);
			}
	exit(1);
}	
