#include <sys/param.h>
#include <machine/prom.h>

#include "bbinfo.h"

extern _end, start;

struct bbinfoloc desc = {
	0xbabefacedeadbeef,
	(u_int64_t)&start,
	(u_int64_t)&_end,
	0xdeadbeeffacebabe
};

int
open_dev(fd)
	int *fd;
{
	prom_return_t ret;
	char devname[64];
	int devlen;

	/*
	 * XXX
         * We don't know what device names look like yet,
         * so we can't change them.
         */
        ret.bits = prom_getenv(PROM_E_BOOTED_DEV, devname, sizeof(devname));
        devlen = ret.u.retval;

        ret.bits = prom_open(devname, devlen);
        if (ret.u.status)
                return 0;

	*fd = ret.u.retval;

	return 1;
}

int
load_file(bbinfop, loadaddr)
	struct bbinfo *bbinfop;
	char *loadaddr;
{
	prom_return_t ret;
	int32_t cksum, *int32p;
	int i, n, fd, rv;
	char *cp;

	if (bbinfop->nblocks <= 0) {
		puts("invalid number of blocks in boot program description\n");
		return 0;
	}
	if (bbinfop->bsize < 4096 || bbinfop->bsize > MAXBSIZE) {
		puts("invalid block size in boot program description\n");
		return 0;
	}

	int32p = (int32_t *)&_end;
	n = bbinfop->nblocks +
	    (sizeof(*bbinfop) / sizeof(bbinfop->blocks[0])) - 1;
	if ((long)&_end - (long)&start + sizeof(bbinfop->blocks[0]) * n >
	    15 * 512) {
		puts("way too many blocks\n");
		return 0;
	}

	for (i = 0, cksum = 0; i < n; i++)
		cksum += *int32p++;
	if (cksum != 0) {
		puts("invalid checksum in boot program description\n");
		return 0;
	}

	if (!open_dev(&fd)) {
		puts("couldn't open disk device\n");
		return 0;
	}

	cp = loadaddr;
	rv = 1;
	for (i = 0; i < bbinfop->nblocks; i++) {
puts(".");
		ret.bits = prom_read(fd, bbinfop->bsize, cp,
		    bbinfop->blocks[i]);
puts("\b");
		if (ret.u.status) {
			rv = 0;
			puts("BLOCK READ ERROR!\n");
			break;
		}
		cp += bbinfop->bsize;
	}
	prom_close(fd);

	return (rv);
}

void
main()
{
	struct bbinfo *bbinfop;
	char *loadaddr;
	void (*entry) __P((void));

	/* Init prom callback vector. */
	init_prom_calls();

	puts("\nNetBSD/Alpha primary boot...\n");

	bbinfop = (struct bbinfo *)&_end;
	loadaddr = (char *)SECONDARY_LOAD_ADDRESS;
	if (!load_file(bbinfop, loadaddr)) {
		puts("\nLOAD FAILED!\n\n");
		return;
	}

	puts("Jumping to entry point...\n");
	entry = (void (*)())loadaddr;
	(*entry)();
	puts("SECONDARY BOOT BLOCK RETURNED!\n");
}
