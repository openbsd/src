#include <sys/types.h>
#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/exec.h>
#include <machine/prom.h>

#define RB_NOSYM 0x400

void parse_args __P((struct mvmeprom_args *pbugargs));
int load_kern();
int read_tape_block __P((short ctrl, short dev, short *status,
    void *addr, int *cnt, int blk_num, u_char *flags, int verbose));

struct kernel {
	void   *entry;
	void   *symtab;
	void   *esym;
	int     bflags;
	int     bdev;
	char   *kname;
	void   *smini;
	void   *emini;
	u_int   end_loaded;
} kernel;

typedef(*kernel_entry) __P((struct mvmeprom_args *, struct kernel *));

int
main(pbugargs)
	struct mvmeprom_args *pbugargs;
{
	kernel_entry addr;

	/*
	print_bugargs(pbugargs);
	print_time();
	print_brdid();
	print_memory();
	*/
	parse_args(pbugargs);
	if (load_kern(pbugargs) == 1) {
		printf("unsuccessful in loading kernel\n");
	} else {
		addr = kernel.entry;

		printf("kernel loaded at %x\n", addr);
		printf("kernel.entry %x\n", kernel.entry);
		printf("kernel.symtab %x\n", kernel.symtab);
		printf("kernel.esym %x\n", kernel.esym);
		printf("kernel.bflags %x\n", kernel.bflags);
		printf("kernel.bdev %x\n", kernel.bdev);
		if (kernel.kname)
			printf("kernel.kname <%s>\n", kernel.kname);
		else
			printf("kernel.kname <null>\n");
		printf("kernel.end_loaded %x\n", kernel.end_loaded);

		if (kernel.bflags & RB_MINIROOT)
			loadmini(kernel.end_loaded, pbugargs);

		printf("kernel.smini %x\n", kernel.smini);
		printf("kernel.emini %x\n", kernel.emini);
		printf("kernel.end_loaded %x\n", kernel.end_loaded);
		if (kernel.bflags & RB_HALT)
			mvmeprom_return();
		if (((u_long)addr &0xf) == 0x2) {
			(addr)(pbugargs, &kernel);
		} else {
			/* is type fixing anything like price fixing? */
			typedef (* kernel_start) __P((int, int, void *,void *, void *));
			kernel_start addr1;
			addr1 = (void *)addr;
			(addr1)(kernel.bflags, 0, kernel.esym, kernel.smini, kernel.emini
	);
		}

	}
	return (0);
}

#define MVMEPROM_SCALE (512/MVMEPROM_BLOCK_SIZE)

int
read_tape_block(ctrl, dev, status, addr, cnt, blk_num, flags, verbose)
	short   ctrl;
	short   dev;
	short  *status;
	void   *addr;
	int    *cnt;
	int     blk_num;
	u_char	*flags;
	int     verbose;
{
	struct mvmeprom_dskio dio;
	int     ret;

	dio.ctrl_lun = ctrl;
	dio.dev_lun = dev;
	dio.status = *status;
	dio.pbuffer = addr;
	dio.blk_num = blk_num;
	dio.blk_cnt = *cnt / (512 / MVMEPROM_SCALE);
	dio.flag = *flags;
	dio.addr_mod = 0;

	if (verbose)
		printf("saddr %x eaddr %x", dio.pbuffer,
		    (int) dio.pbuffer + (dio.blk_cnt * MVMEPROM_BLOCK_SIZE));
	ret = mvmeprom_diskrd(&dio);

	*status = dio.status;
	*cnt = (dio.blk_cnt / MVMEPROM_SCALE) * 512;
	if (verbose) {
		printf("status %x ret %d ", *status, ret);
		printf("flags %x blocks read %x cnt %x\n",
		    *flags, dio.blk_cnt, *cnt);
	}
	return (ret);
}
#ifdef DEBUG
int     verbose = 1;
#else
int     verbose = 0;
#endif

int
load_kern(pbugargs)
	struct mvmeprom_args *pbugargs;
{
	int     ret;
	char   *addr;
	u_char flags;
	short   status = 0;
	int     blk_num;
	struct exec *pexec;
	int     magic;
	int    *esym;
	int    *symtab;
	int     cnt, len;
	char    buf[512];

	blk_num = 2;
	/* flags = IGNORE_FILENUM; */
	flags = 0;
	cnt = 512;
printf("ctrl %x dev %x\n",pbugargs->ctrl_lun, pbugargs->dev_lun);
	ret = read_tape_block(pbugargs->ctrl_lun, pbugargs->dev_lun, &status,
	    buf, &cnt, blk_num, &flags, verbose);
	if (ret != 0) {
		printf("unable to load kernel 1 status %x\n", status);
		return (1);
	}
	pexec = (struct exec *) buf;
	if (N_GETMID(*pexec) != MID_M68K &&
	    N_GETMID(*pexec) != MID_M68K4K) {
		printf("invalid mid on kernel\n");
		return (1);
	}

	magic = N_GETMAGIC(*pexec);
	switch (magic) {
	case ZMAGIC:
		break;
	case NMAGIC:
		printf("NMAGIC not yet supported");
	case OMAGIC:
	case QMAGIC:
	default:
		printf("Unknown or unsupported magic type <%x>\n", magic);
		return (1);
	}
	if (magic == ZMAGIC) {
		status = 0;
		addr = (char *) (pexec->a_entry & ~0x0FFF);

		if ((int) pexec->a_entry != (int) addr + 0x22) {
			printf("warning kernel start address not %x, %x\n",
			    (int) addr + 0x22, pexec->a_entry);
			printf("kernel loaded at %x\n", addr);
		}
		bcopy(&buf, addr, 512);
		/* 2nd block of exe */
		addr += 512;

		printf("text 0x%x data 0x%x bss 0x%x\n",
		    pexec->a_text, pexec->a_data, pexec->a_bss);

		len = (pexec->a_text - 512);	/* XXX */
		len += (pexec->a_data);

		printf("loading [ %x + %x ", pexec->a_text, pexec->a_data);

		cnt = len;
		flags = IGNORE_FILENUM;
		ret = read_tape_block(pbugargs->ctrl_lun, pbugargs->dev_lun,
		    &status, addr, &cnt, blk_num, &flags, verbose);
		if (ret != 0 || cnt != len) {
			printf("unable to load kernel 2 status %x\n", status);
			return 1;
		}
		addr += len;

		/* Skip over text and data and zero bss. */
		len = pexec->a_bss;
		printf("+ %x", len);
#ifdef DEBUG
		printf("bss %x - %x\n", addr, addr + pexec->a_bss);
#endif
		bzero(addr, pexec->a_bss);
		addr += len;

		if (pexec->a_syms != 0 && !(kernel.bflags & RB_NOSYM)) {
			printf(" + [ %x", pexec->a_syms);
			addr += 4;	/* skip over _end symbol */
			symtab = (void *) pexec->a_syms;
			len = pexec->a_syms;
			cnt = ((len + (512 - 1)) / 512) * 512;
			flags = IGNORE_FILENUM;
			ret = read_tape_block(pbugargs->ctrl_lun,
			    pbugargs->dev_lun, &status, addr,
			    &cnt, blk_num, &flags, verbose);
			if (ret != 0 || cnt != ((len + (512 - 1)) / 512) * 512) {
				printf("unable to load kernel 3\n");
				return 1;
			}
			/* this value should have already been loaded XXX */
			esym = (void *) ((u_int) addr + pexec->a_syms);
			if ((int) addr + cnt <= (int) esym) {
				printf("missed loading count of symbols\n");
				return 1;
			}
			addr += cnt;


			len = *esym;
#if 0
			printf("start load %x end load %x %x\n", addr,
			    len, addr + len);
			printf("esym %x *esym %x\n", esym, len);
#endif
			/* dont load tail of already loaded */
			len -= (u_int) addr - (u_int) esym;

			if (len > 0) {
				printf(" + %x", *esym);
				esym = (void *) (addr + len);
				cnt = ((len + (512 - 1)) / 512) * 512;
				flags = IGNORE_FILENUM;
				ret = read_tape_block(pbugargs->ctrl_lun,
				    pbugargs->dev_lun, &status, addr,
				    &cnt, blk_num, &flags, verbose);
				if (ret != 0 || cnt != ((len + (512-1)) / 512)*512) {
					printf("unable to load kernel 4\n");
					return (1);
				}
				addr += len;
				printf(" ]");
			} else {
				printf("+ %x ]", *esym);
			}
			esym = (int *) (((int) esym) + *esym);

			kernel.symtab = symtab;
			kernel.esym = esym;
		} else {
			kernel.symtab = 0;
			kernel.esym = 0;
		}
		kernel.end_loaded = (int) addr;
		flags = IGNORE_FILENUM | END_OF_FILE;
		cnt = 8192;
		printf("removing pad [");
		ret = read_tape_block(pbugargs->ctrl_lun, pbugargs->dev_lun,
		    &status, addr, &cnt, blk_num, &flags, verbose);
		if (ret != 0) {
			printf("unable to load kernel 5\n");
			return (1);
		}
		printf(" %d ]", cnt);

		printf("]\n");
	}
	kernel.entry = (void *) pexec->a_entry;
	return (0);
}

int
loadmini(addr, pbugargs)
	u_int addr;
	struct mvmeprom_args *pbugargs;
{
	int cnt, ret, blk_num = 3;
	short status = 0;
	u_char flags;

	/* align addr to some boundary */
#define ALIGN_F 0x4
	addr = (u_int) ((((int) addr + ALIGN_F - 1) / ALIGN_F) * ALIGN_F);
#undef ALIGN_F
	flags = END_OF_FILE;
	cnt = 6144 * 512;	/* some abserdly large value. (3meg) */
	printf("loading miniroot[ ");
	ret = read_tape_block(pbugargs->ctrl_lun, pbugargs->dev_lun,
	    &status, (void *) addr, &cnt, blk_num, &flags, verbose);
	if (ret != 0) {
		printf("unable to load miniroot\n");
		return (1);
	}
	kernel.smini = (void *)addr;
	printf("%d ]\n", cnt);
	kernel.emini = (void *) ((u_int) addr + cnt);
	kernel.end_loaded = (u_int) kernel.emini;
	return (0);
}

void
parse_args(pargs)
	struct mvmeprom_args *pargs;
{
	char *ptr = pargs->arg_start;
	char c, *name = NULL;
	int howto = 0;

	if (pargs->arg_start != pargs->arg_end) {
		while (c = *ptr) {
			while (c == ' ')
				c = *++ptr;
			if (!c)
				return;
			if (c != '-') {
				name = ptr;
				while ((c = *++ptr) && c != ' ');
				if (c)
					*ptr++ = 0;
				continue;
			}
			while ((c = *++ptr) && c != ' ') {
				if (c == 'a')
					howto |= RB_ASKNAME;
				else if (c == 'b')
					howto |= RB_HALT;
				else if (c == 'y')
					howto |= RB_NOSYM;
				else if (c == 'd')
					howto |= RB_KDB;
				else if (c == 'm')
					howto |= RB_MINIROOT;
				else if (c == 'r')
					howto |= RB_DFLTROOT;
				else if (c == 's')
					howto |= RB_SINGLE;
			}
		}
	}
	kernel.bflags = howto;
	kernel.kname = name;
}
