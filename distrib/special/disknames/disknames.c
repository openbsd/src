#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>

#include <stdlib.h>

int
main() {
	int mib[2];
	size_t len;
	char *p;

	mib[0] = CTL_HW;
	mib[1] = HW_DISKNAMES;

	if (sysctl(mib, 2, NULL, &len, NULL, 0) != -1)
		if ((p = (char *)malloc(len)) != NULL)
			if (sysctl(mib, 2, p, &len, NULL, 0) != -1) {
				write(1, p, len);
				_exit(0);
			}	
	_exit(1);
}	
