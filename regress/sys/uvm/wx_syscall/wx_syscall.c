#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

int
main()
{
	vaddr_t o = (vaddr_t)flock;
	int psz = getpagesize();

	printf("%llx\n", (long long)flock);
	if (mprotect((void *)(o & ~(psz-1)), psz,
	    PROT_EXEC|PROT_WRITE|PROT_READ) == -1 &&
	    errno == ENOTSUP) {
		printf("mprotect -> ENOTSUP?  Please run from wxallowed filesystem\n");
		exit(0);
	}
	flock(0, 0);

	printf("performing syscall succeeded.  Should have been killed.\n");
}
