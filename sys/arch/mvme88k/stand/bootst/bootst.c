/*
 * Changes Copyright (c) 1998 steve Murphree, Jr.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/exec.h>
#include <sys/param.h>
#include <sys/reboot.h>
#include <machine/prom.h>

#include "stand.h"
#include "libsa.h"

#ifndef RB_NOSYM
#define RB_NOSYM 0x400
#endif

void parse_bugargs __P((struct mvmeprom_args *pbugargs));
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

struct mvmeprom_brdid2 {
	u_long	eye_catcher;
	u_char	rev;
	u_char	month;
	u_char	day;
	u_char	year;
	u_short	size;
	u_short	rsv1;
	u_short	model;
	u_short	suffix;
	u_long	options:24;
	u_char	family:4;
	u_char	cpu:4;
	u_short	ctrlun;
	u_short	devlun;
	u_short	devtype;
	u_short	devnum;
	u_long	bug;
	u_char	xx1[16];
	u_char	xx2[4];
	u_char	longname[12];
	u_char	xx3[16];
	u_char	speed[4];
	u_char	xx4[12];
};
extern 	char *version;

int
main()
{
	kernel_entry addr;
	struct mvmeprom_brdid2 *brdid;
	brdid = (struct mvmeprom_brdid2 *) mvmeprom_brdid();
	printf(">> OpenBSD stboot [%s]\n\n", version);
	printf("   MVME%x%xs\n", brdid->model, brdid->suffix);
	printf("   Booting from Controler %x, Drive %x\n", brdid->ctrlun, brdid->devlun);
	printf("   Speed %s\n", brdid->speed);
	if (bugargs.arg_start != bugargs.arg_end)
	    printf("   Args %s\n", bugargs.arg_start);
        printf("\n");
	
	parse_bugargs(&bugargs);
	if (load_kern(bugargs) == 1) {
		printf("unsuccessful in loading kernel\n");
	} else {
		addr = kernel.entry;
		if(kernel.esym == 0) kernel.esym = (int*)kernel.end_loaded;
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
			loadmini(kernel.end_loaded, bugargs);

		printf("kernel.smini %x\n", kernel.smini);
		printf("kernel.emini %x\n", kernel.emini);
		printf("kernel.end_loaded %x\n", kernel.end_loaded);
		if (kernel.bflags & RB_HALT)
			mvmeprom_return();
		if (((u_long)addr &0xf) == 0x2) {
			(addr)(&bugargs, &kernel);
		} else {
			/* is type fixing anything like price fixing? */
			typedef (* kernel_start) __P((int, int, void *,void *, void *));
			kernel_start addr1;
			addr1 = (void *)addr;
			(addr1)(kernel.bflags, bugargs.ctrl_addr, kernel.esym, kernel.smini, kernel.emini);
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
		printf("saddr %x eaddr %x ", dio.pbuffer,
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
	int     len2;
	int     magic;
	int    *esym;
	int    *symtab;
	int     cnt, len, endsym;
	char    buf[512];
	int mid = 0;

	blk_num = 2;
	/*flags = IGNORE_FILENUM;*/
	flags = 0;
	cnt = 512;
	
/*	printf("ctrl %x dev %x\n",pbugargs->ctrl_lun, pbugargs->dev_lun);*/

	ret = read_tape_block(pbugargs->ctrl_lun, pbugargs->dev_lun, &status,
        buf, &cnt, blk_num, &flags, verbose);
	
	if (ret != 0 && status != 0x300) {
		printf("unable to load kernel 1 status %x\n", status);
		return (1);
	}
	pexec = (struct exec *) buf;

	mid = N_GETMID(*pexec);
	if ( mid != MID_M88K ) {
		printf("invalid mid %d on kernel\n", mid);
		return (1);
	}

	magic = N_GETMAGIC(*pexec);
	switch (magic) {
	case ZMAGIC:
		break;
	case NMAGIC:
		printf("NMAGIC not yet supported");
	case OMAGIC:
		printf("OMAGIC file\n");
	case QMAGIC:
		printf("QMAGIC file\n");
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
			printf("\nunable to load kernel 2 status %x\n", status);
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
				printf("\nunable to load kernel 3\n");
				return 1;
			}
			/* this value should have already been loaded XXX */
			esym = (void *) ((u_int) addr + pexec->a_syms);
			if ((int) addr + cnt <= (int) esym) {
				printf("\nmissed loading count of symbols\n");
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
				esym = (int*) (addr + len);
				cnt = ((len + (512 - 1)) / 512) * 512;
				flags = IGNORE_FILENUM;
				ret = read_tape_block(pbugargs->ctrl_lun,
				    pbugargs->dev_lun, &status, addr,
				    &cnt, blk_num, &flags, verbose);
				if (ret != 0 || cnt != ((len + (512-1)) / 512)*512) {
					printf("\nunable to load kernel 4\n");
					return (1);
				}
				addr += len;
				printf(" ]");
			} else {
				printf("+ %x ]", *esym);
			}
			esym = (int*)addr;

			kernel.symtab = symtab;
			kernel.esym = esym;
		} else {
			kernel.symtab = 0;
			kernel.esym = 0;
		}
		printf(" removing pad [");

		kernel.end_loaded = (int) addr;
		flags = IGNORE_FILENUM | END_OF_FILE;
		cnt = 8192;
		ret = read_tape_block(pbugargs->ctrl_lun, pbugargs->dev_lun,
		    &status, addr, &cnt, blk_num, &flags, verbose);
		if (ret != 0) {
			printf("\nunable to load kernel 5\n");
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
		printf("\nunable to load miniroot\n");
		return (1);
	}
	kernel.smini = (void *)addr;
	printf("%d ]\n", cnt);
	kernel.emini = (void *) ((u_int) addr + cnt);
	kernel.end_loaded = (u_int) kernel.emini;
	return (0);
}

void
parse_bugargs(pargs)
	struct mvmeprom_args *pargs;
{
	char *ptr = pargs->arg_start;
	char c, *name = "bsd";
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
