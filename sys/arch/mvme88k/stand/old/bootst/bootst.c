#include "bug.h"
#include <sys/types.h>
#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/exec.h>
/*
#include <sys/exec_aout.h>
*/

#define KERNEL_LOAD_ADDRESS ((void *)0x4000)
#define BUG_BLOCK_SIZE 512
#define VERSION 0x0000

#define RB_NOSYM 0x400



void memset(void *,char,size_t);
void printf(char *,...);
void parse_args(struct bugargs *pbugargs);
int read_tape_block(short ctrl, short dev, short *status, void *addr,
		int *cnt, int blk_num, unsigned char *flags,int verbose);
int load_kern();

struct kernel {
	void *entry;
	void *symtab;
	void *esym;
	int	 bflags;
	int	 bdev;
	char *kname;
	void *smini;
	void *emini;
	u_int end_loaded;
} kernel;

typedef (* kernel_entry)(struct bugargs *,struct kernel *);

void main(struct bugargs *pbugargs)
{
	kernel_entry addr;

	/*
	print_bugargs(pbugargs);
	print_time();
	print_brdid();
	print_memory();
	*/
	parse_args(pbugargs);
	if (1 == load_kern(pbugargs)) {
		printf("unsuccessful in loading kernel\n\r");
	} else {
		addr = kernel.entry;
		printf("kernel loaded at %x\n\r",addr);
		printf("kernel.entry %x\n\r",kernel.entry);
		printf("kernel.symtab %x\n\r",kernel.symtab);
		printf("kernel.esym %x\n\r",kernel.esym);
		printf("kernel.bflags %x\n\r",kernel.bflags);
		printf("kernel.bdev %x\n\r",kernel.bdev);
		if (kernel.kname) {
		printf("kernel.kname <%s>\n\r",kernel.kname);
		} else {
		printf("kernel.kname <null>\n\r");
		}
		printf("kernel.end_loaded %x\n\r",kernel.end_loaded);
		if (kernel.bflags & RB_MINIROOT) {
			loadmini(kernel.end_loaded,pbugargs);
		}
		printf("kernel.smini %x\n\r",kernel.smini);
		printf("kernel.emini %x\n\r",kernel.emini);
		printf("kernel.end_loaded %x\n\r",kernel.end_loaded);
		if (*pbugargs->arg_start == 'e')
			bug_return();
		(addr)(pbugargs,&kernel);
	}

	return;
}
int
read_tape_block(short ctrl, short dev, short *status, void *addr,
		int *cnt, int blk_num, unsigned char *flags,int verbose)
{
	struct bug_dskio dio;
	int ret;

	dio.ctrl_lun	= ctrl;
	dio.dev_lun	= dev;
	dio.status	= *status;
	dio.pbuffer	= addr;
	dio.blk_num	= blk_num;
	dio.blk_cnt	= *cnt * 2;
	dio.flag	= *flags;
	dio.addr_mod	= 0;

	if (verbose){
		printf("saddr %x eaddr %x", dio.pbuffer, 
			(int)dio.pbuffer + (dio.blk_cnt * BUG_BLOCK_SIZE/2 ));
	}
	
	ret	=  bug_diskrd(&dio);

	*status	= dio.status;
	*cnt	= dio.blk_cnt/2;
	if (verbose) {
		printf("status %x ret %d ",*status, ret);
		printf("flags %x\n\r",*flags);
	}
	return ret;
}
int load_kern(struct bugargs *pbugargs)
{
	int ret;
	char *addr;
	unsigned char flags;
	short status = 0;
	int verbose = 0;
	int blk_num;
	struct exec *pexec;
	int magic;
	int *esym;
	int *symtab;
	int cnt, len;

	blk_num = 0;
	flags = IGNORE_FILENUM ;
	cnt = 512 / BUG_BLOCK_SIZE;
	addr = KERNEL_LOAD_ADDRESS;
	ret = read_tape_block(pbugargs->ctrl_lun, pbugargs->dev_lun, &status, addr,
			&cnt, blk_num, &flags, verbose);
	if (ret != 0) {
		printf("unable to load kernel\n\r");
		return 1;
	}
	pexec = (struct exec *) addr;
	if ((N_GETMID(*pexec)  != MID_M68K) && 
		( N_GETMID(*pexec)  != MID_M68K4K ))
	{
		printf("invalid mid on kernel\n\r");
		return 1;
	}
	{
		short *pversion = (void *)0x4020;
		if (VERSION != *pversion) {
			printf("invalid version of kernel/loader\n\r");
			bug_return();
		}
	}
	magic = N_GETMAGIC(*pexec);
	switch (magic) {
		case ZMAGIC:
			break;
		case NMAGIC:
			printf ("NMAGIC not yet supported");
		case OMAGIC:
		case QMAGIC:
		default:
			printf("Unknown or unsupported magic type <%x>\n\r",
				magic);
			return 1;
			break;
	}
	if ( magic == ZMAGIC ) {

		status = 0;
		/* 2nd block of exe */
		addr += 512;

		if ((int)pexec->a_entry != (int)KERNEL_LOAD_ADDRESS + 0x22) {
			printf ("warning kernel start address not %x, %x\n\r",
				(int)KERNEL_LOAD_ADDRESS + 0x22,pexec->a_entry);
			printf ("kernel loaded at %x\n\r",KERNEL_LOAD_ADDRESS);

		}
		printf ("text 0x%x data 0x%x bss 0x%x\n\r",
			pexec->a_text, pexec->a_data, pexec->a_bss);

		len = (pexec->a_text - 512) ; /* XXX */
		len += (pexec->a_data );

		printf ("loading [ %x + %x ",pexec->a_text,pexec->a_data);

		cnt = (len + BUG_BLOCK_SIZE -1)/ BUG_BLOCK_SIZE;
		flags = IGNORE_FILENUM ;
		ret = read_tape_block(pbugargs->ctrl_lun, pbugargs->dev_lun, &status, addr,
				&cnt, blk_num, &flags, verbose);
		if (ret != 0 || cnt != (len + BUG_BLOCK_SIZE -1)/ BUG_BLOCK_SIZE) {
			printf("unable to load kernel\n\r");
			return 1;
		}
		addr += len;

		/* Skip over text and data and zero bss. */
		len = pexec->a_bss;
		printf ("+ %x",len);
		memset (KERNEL_LOAD_ADDRESS + (pexec->a_text + pexec->a_data),
			0, pexec->a_bss);
		addr +=len;
		
		if (pexec->a_syms != 0 && !(kernel.bflags & RB_NOSYM)) {
			printf (" + [ %x",pexec->a_syms);
			/* align addr */
#if 0
#define ALIGN_F 0x200
			addr = (void *)((((int)addr + ALIGN_F -1)/ALIGN_F) * ALIGN_F);
#endif
			addr += 4;  /* skip over _end symbol */
			symtab = (void *)pexec->a_syms;
			len = pexec->a_syms;
			cnt = (len+(BUG_BLOCK_SIZE-1)) / BUG_BLOCK_SIZE;
			flags = IGNORE_FILENUM ;
			ret = read_tape_block(pbugargs->ctrl_lun, pbugargs->dev_lun, &status, addr,
					&cnt, blk_num, &flags, verbose);
			if (ret != 0 || cnt != (len+(BUG_BLOCK_SIZE-1)) / BUG_BLOCK_SIZE)
			{
				printf("unable to load kernel\n\r");
				return 1;
			}

			/* this value should have already been loaded XXX */
			esym = (void *) ((u_int)addr + pexec->a_syms);
			if ((int)addr +(cnt * BUG_BLOCK_SIZE) <= (int) esym) {
				printf("missed loading count of symbols\n\r");
				return 1;
			}
			addr +=cnt * BUG_BLOCK_SIZE;


			len = *esym;
#if 0
			printf("start load %x end load %x %x\n\r", addr,
				len, addr +len);
			printf("esym %x *esym %x\n\r",esym, len);
#endif
			/* dont load tail of already loaded */
			len -= (u_int)addr - (u_int)esym;

			if (len > 0) {
				printf(" + %x",*esym);
				esym = (void *)(addr + len);
				cnt = (len+(BUG_BLOCK_SIZE-1)) / BUG_BLOCK_SIZE;
				flags = IGNORE_FILENUM ;
				ret = read_tape_block(pbugargs->ctrl_lun, pbugargs->dev_lun, &status, addr,
						&cnt, blk_num, &flags, verbose);
				if (ret != 0 ||
					cnt != (len+(BUG_BLOCK_SIZE-1)) / BUG_BLOCK_SIZE)
				{
					printf("unable to load kernel\n\r");
					return 1;
				}
				addr += len;
				printf(" ]");
			} else {
				printf("+ %x ]",*esym);
			}
			esym = (int *)(((int)esym) + *esym);

			kernel.symtab = symtab;
			kernel.esym = esym;
		} else {
			kernel.symtab = 0;
			kernel.esym = 0;
		}
		kernel.end_loaded = (int)addr;
		flags = IGNORE_FILENUM | END_OF_FILE;
		cnt = 1000;
		printf ("removing pad [");
		ret = read_tape_block(pbugargs->ctrl_lun, pbugargs->dev_lun, &status, addr,
				&cnt, blk_num, &flags, verbose);
		if (ret != 0) {
			printf("unable to load kernel\n\r");
			return 1;
		}
		printf (" %d ]",cnt * BUG_BLOCK_SIZE);

		printf("]\n\r");
	}


	kernel.entry =  (void *)pexec->a_entry;
	return 0;
}
loadmini(u_int addr,struct bugargs *pbugargs)
{
	int ret;
	unsigned char flags;
	short status = 0;
	int verbose = 0;
	int blk_num;
	int cnt;
	blk_num = 3;
	/*
	flags = IGNORE_FILENUM | END_OF_FILE;
	*/
	/* align addr to some boundary */
#define ALIGN_F 0x4
	addr = (u_int)((((int)addr + ALIGN_F -1)/ALIGN_F) * ALIGN_F);
#undef ALIGN_F
	flags = END_OF_FILE;
	cnt = 6144; /* some abserdly large value. (3meg / 512) */
	printf("loading miniroot[ ");
	ret = read_tape_block(4, pbugargs->dev_lun, &status, (void*)addr,
			&cnt, blk_num, &flags, verbose);
	if (ret != 0) {
		printf("unable to load miniroot\n\r");
		return 1;
	}
	kernel.smini = (void *)addr;
	printf("%d ]\n\r",(BUG_BLOCK_SIZE * cnt));
	kernel.emini = (void*)((u_int)addr + (BUG_BLOCK_SIZE * cnt));
	kernel.end_loaded = (u_int)kernel.emini;
}
void
parse_args(struct bugargs *pargs)
{
	char * ptr = pargs->arg_start;
	char c, *name;
	int howto;
	howto = ( 0 | RB_DFLTROOT );
	name = NULL;

	if (pargs->arg_start != pargs->arg_end) {
		while (c = *ptr) {
			while (c == ' ')
				c = *++ptr;
			if (!c)
				return;
			if (c == '-')
				while ((c = *++ptr) && c != ' ') {
					if (c == 'a')
						howto |= RB_ASKNAME;
					else if (c == 'b')
						howto |= RB_HALT;
					else if (c == 'y')
						howto |= RB_NOSYM;
#ifdef CHECKSUM
					else if (c == 'c')
						cflag = 1;
#endif
					else if (c == 'd')
						howto |= RB_KDB;
					else if (c == 'm')
						howto |= RB_MINIROOT;
					else if (c == 'r')
/* change logic to have force root to config device UNLESS arg given */
						howto &= ~RB_DFLTROOT;
					else if (c == 's')
						howto |= RB_SINGLE;
				}
			else {
				name = ptr;
				while ((c = *++ptr) && c != ' ');
				if (c)
					*ptr++ = 0;
			}
		}
		if (RB_NOSYM & howto) printf("RB_NOSYM\n\r");
		if (RB_AUTOBOOT & howto) printf("RB_AUTOBOOT\n\r");
		if (RB_SINGLE & howto) printf("RB_SINGLE\n\r");
		if (RB_NOSYNC & howto) printf("RB_NOSYNC\n\r");
		if (RB_HALT & howto) printf("RB_HALT\n\r");
		if (RB_DFLTROOT & howto) printf("RB_DFLTROOT\n\r");
		if (RB_KDB & howto) printf("RB_KDB\n\r");
		if (RB_RDONLY & howto) printf("RB_RDONLY\n\r");
		if (RB_DUMP & howto) printf("RB_DUMP\n\r");
		if (RB_MINIROOT & howto) printf("RB_MINIROOT\n\r");

	}
	kernel.bflags = howto;
	kernel.kname = name;
}

