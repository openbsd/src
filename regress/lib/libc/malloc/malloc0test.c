#include <sys/types.h>
#include <sys/signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <setjmp.h>

volatile sig_atomic_t got;
jmp_buf jmp;

void
catch(int signo)
{
	got++;
	longjmp(jmp, 0);
}

int
test(caddr_t p, int size)
{
	signal(SIGSEGV, catch);
	got = 0;
	if (setjmp(jmp) == 0)
		*p = 0;
	if (setjmp(jmp) == 0)
		*(p+size-1) = 0;
	return (got);
}

char *prot_table[] = {
	"unprotected",
	"fuckup",
	"protected"
};

#define SIZE	10

/*
 * Do random memory allocations.
 *
 * For each one, ensure that it is at least 16 bytes in size (that
 * being what our current malloc returns for the minsize of an
 * object, alignment wise);
 *
 * For zero-byte allocations, check that they are still aligned.
 *
 * For each object, ensure that they are correctly protected or not
 * protected.
 *
 * Does not regress test malloc + free combinations ... it should.
 */
int
main(int argc, char *argv[])
{
	caddr_t rblob = malloc(1);
	caddr_t zblob = malloc(0);
	caddr_t *blobp, blob;
	int size, rsize, tsize;
	int count = 0, prot;
	int rval = 0, fuckup = 0;

	while (1) {
		size = arc4random() % SIZE;
		blob = malloc(size);
		if (blob == NULL) {
			fprintf(stderr, "success: out of memory\n");
			exit(rval);
		}

		if (size == 0) {
			blobp = &zblob;
			tsize = 16;
		} else {
			blobp = &rblob;
			tsize = size;
		}

		rsize = blob - *blobp;
		fuckup = SIZE < 16 && size >= rsize;
		prot = test(blob, tsize);

		if (size == 0 && rsize < 16)
			fuckup = 1;
		if (size == 0 && prot < 2)
			fuckup = 1;

		if (fuckup) {
			printf("%8p %6d %6d %20s %10s\n", blob, size, rsize,
			    prot_table[prot], fuckup ? "fuckup" : "");
			rval = 1;
		}
		*blobp = blob;


		if (++count % 100000 == 0)
			fprintf(stderr, "count = %d\n", count);
	}
}