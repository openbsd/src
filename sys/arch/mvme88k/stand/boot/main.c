#include <sys/param.h>
#include <sys/reboot.h>
#include "bug.h"
#include "bugio.h"
#include "machine/exec.h"

int	readblk		__P((int, char *));
int	loados		__P((void));	
void	putchar		__P((char));
void	_main		__P((void));
void	tapefileseek	__P((int));

char Clun, Dlun;

#define	DEV_BSIZE		512
#define	KERNEL_LOAD_ADDR	0x10000
#if !defined(BUG_BLKSIZE)
#define	BUG_BLKSIZE		256
#endif /* BUG_BLKSIZE */
#define	sec2blk(x)	((x) * (DEV_BSIZE/BUG_BLKSIZE))

struct kernel {
	void *entry;
	void *symtab;
	void *esym;
	int	 bflags;
	int	 bdev;
	char *kname;
	void *smini;
	void *emini;
	unsigned int end_loaded;
} kernel;

int howto = 0;
int bootdev = 0;
int *miniroot;

void
putchar(char c)
{
	bugoutchr(c);
}

main(struct bugenv *env)
{
	printf("Clun %x Dlun %x\n", env->clun, env->dlun);
	Clun = (char)env->clun;
	Dlun = (char)env->dlun;
	loados();
	return;
}


loados(void)
{
	int i, size;
	register char *loadaddr = (char *)KERNEL_LOAD_ADDR; /* load addr 64k*/
	struct exec *hdr;
	int (*fptr)();
	int *esym;
	int cnt, strtablen, ret;
	char *addr;

	howto |= RB_SINGLE|RB_KDB;

	tapefileseek(2);	/* seek to file 2 - the OS */
	if (readblk(1, loadaddr) == -1) {
		printf("Unable to read blk 0\n");
		return 1;
	}
	hdr 	= (struct exec *)loadaddr;
	
	/* We only deal with ZMAGIC files */
	if ((int)hdr->a_entry != (int)(loadaddr + sizeof(struct exec))) {
		printf("a_entry != loadaddr + exec size\n");
	}
	size = hdr->a_text + hdr->a_data;
	size -= DEV_BSIZE; /* account for the block already read */

	printf("Loading [%x+%x", hdr->a_text, hdr->a_data);
	if (readblk(size / DEV_BSIZE, loadaddr + DEV_BSIZE) == -1) {
		printf("Error reading the OS\n");
		return 1;
	}

	/* zero out BSS */

	printf("+%x]", hdr->a_bss);
	printf("zero'd out %x (%x)\n", loadaddr + hdr->a_text + hdr->a_data,
			hdr->a_bss);
	/*memset(loadaddr + hdr->a_text + hdr->a_data, 0, hdr->a_bss); */
	bzero(loadaddr + hdr->a_text + hdr->a_data, hdr->a_bss);

	addr = loadaddr + hdr->a_text + hdr->a_data + hdr->a_bss;

	if (hdr->a_syms != 0 /* && !(kernel.bflags & RB_NOSYM)*/) {
		/*
		 * DDB expects the following layout:
		 * 	no. of syms
		 *	symbols
		 *	size of strtab
		 *	entries of strtab
		 * esym->...
		 * Where as size of strtab is part of strtab, we need
		 * to prepend the size of symtab to satisfy ddb.
		 * esym is expected to point past the last byte of
		 * string table, rouded up to an int.
		 */
		bcopy(&hdr->a_syms, addr, sizeof(hdr->a_syms));
		addr += 4;  /* account for a_syms copied above */
		printf (" + [ %x",hdr->a_syms);

		cnt = (hdr->a_syms + DEV_BSIZE - 1) & ~(DEV_BSIZE - 1);

		ret = readblk(cnt / DEV_BSIZE, addr);
		if (ret != 0) {
			printf("unable to load kernel\n");
			return 1;
		}

		esym = (void *) ((int)addr + hdr->a_syms);

		if ((int)addr + cnt <= (int)esym) {
			printf("missed loading count of symbols\n\r");
			return 1;
		}

		addr += cnt;

		strtablen = *esym;
#if 0
		printf("start load %x end load %x %x\n", addr,
			len, addr +len);
		printf("esym %x *esym %x\n",esym, len);
#endif
		/*
		 * If symbol table size is not a sector multiple, we
		 * already read part of the string table. Look at the
		 * part already read, and figure out the string table
		 * size. Also, adjust the size yet to read.
		 */
		if (hdr->a_syms != cnt) {
			/* already read part of the string table */
			strtablen -= (cnt - hdr->a_syms);
		}

		if (strtablen > 0) {
			printf(" + %x",*esym);

			cnt = (strtablen + DEV_BSIZE -1) & ~(DEV_BSIZE - 1);

			ret = readblk(cnt / DEV_BSIZE, addr);
			if (ret != 0) {
				printf("unable to load kernel\n");
				return 1;
			}
			addr += strtablen;
			printf(" ]\n");
		} else {
			printf("+ %x ]\n", *esym);
		}
		esym = (int *)(((int)esym) + *esym);
		esym = (int *)(((int)esym + 4 - 1) & ~3);

		kernel.symtab = (void *)hdr->a_syms;
		kernel.esym = esym;
	} else {
		kernel.symtab = 0;
		kernel.esym = 0;
	}

	kernel.end_loaded = (unsigned int)addr;
	miniroot = (int *)esym;
	miniroot = (int *)(((int)miniroot + 0x1000 - 1) & ~0xFFF);
	tapefileseek(3);	/* seek to file 3 - minroot */
#if 0
	if (readblk(1000, miniroot) != 0) { /* 5 Mb */
		printf("miniroot not loaded\n");
		addr = (char *)miniroot;
	} else {
		addr = (char *)((int)miniroot + 1000 * DEV_BSIZE);
	}
#endif /* 0 */
	readblk(4096, miniroot); /* 2 Mb */
	addr = (char *)((int)miniroot + 4096 * DEV_BSIZE);
	printf("esym %x miniroot @ %x (ends @ %x)\n", esym, miniroot, addr);
#if 0
	{
		char *symaddr = (char *)0x01F00000;
		int  i;

		tapefileseek(4);	/* seek to file 4 - syms */
		readblk(1, symaddr);
		i = *symaddr;
		i = (i * 0x1C + 4 + DEV_BSIZE) & ~(DEV_BSIZE - 1);
		printf("loading %d symbols (%d sectors)\n",
			*symaddr, (i + 1) * DEV_BSIZE);
		readblk(i / DEV_BSIZE, symaddr + DEV_BSIZE);
		readblk(100, 0x01F00000);
	}
#endif

	fptr = (int (*)())hdr->a_entry;
	/*
	 * Args are passed as
	 * 	r2 howto
	 *	r3 end addr
	 *	r4 (Clun << 8) | Dlun & FF
	 *	r5 esym
	 *	r6 miniroot
	 */
	bootdev = ((Clun << 8) & 0xFF00 | Dlun & 0xFF) & 0xFFFF;
#if 0
	asm volatile ("or r2, r0, %0\n\tor r3, r0, %1\n\tor r4, r0, %2\n\tor r5, r0, %3\n\tor r6, r0, %4\n\tor r7, r0, %5"
		: /* no outputs */
		: "r" (howto), "r" (addr), "r" (Clun), "r" (Dlun), "r" (esym), "r" (miniroot)
		: "r2", "r3", "r4", "r5", "r6", "r7");
#endif /* 0 */
	(*fptr)(howto, addr, bootdev, esym, miniroot);	
	return 0;
}

int
readblk(int n, char *addr)
{
	struct bugdisk_io io;

	io.clun = Clun;
	io.dlun = Dlun;
	io.status = 0;
	io.addr = (void *)addr;
	io.fileno = 0; /* for tape reads, start io at current pos */
	io.nblks = sec2blk(n);
	io.flag = IGNOREFILENO;
	io.am = 0;
	bugdskrd(&io);
	if (io.status)
		return -1;
	return 0;
}

void
_main(void)
{
	return;
}

void
tapefileseek(int i)
{
	struct bugdisk_io io;
	void *addr = (void *)KERNEL_LOAD_ADDR; /* some number - don't care */

	io.clun = Clun;
	io.dlun = Dlun;
	io.status = 0;
	io.addr = addr;
	io.fileno = i; /* for tape reads, this is the file no. */
	io.nblks = 0;
	io.flag = 0; /* we want to turn off IFN and EOF bits */
	io.am = 0;
	bugdskrd(&io);
}

__main()
{
}
