/*	$NetBSD: loadbsd.c,v 1.11 1996/01/09 09:55:15 leo Exp $	*/

/*
 * Copyright (c) 1995 L. Weppelman
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
 *      This product includes software developed by Leo Weppelman.
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

/*
 * NetBSD loader for the Atari-TT.
 */

#include <a_out.h>
#include <fcntl.h>
#include <stdio.h>
#include <osbind.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libtos.h"
#include "loader.h"

char	*Progname;		/* How are we called		*/
int	d_flag  = 0;		/* Output debugging output?	*/
int	h_flag  = 0;		/* show help			*/
int	s_flag  = 0;		/* St-ram only			*/
int	t_flag  = 0;		/* Just test, do not execute	*/
int	v_flag  = 0;		/* show version			*/

const char version[] = "$Revision: 1.2 $";

/*
 * Default name of kernel to boot, large enough to patch
 */
char	kname[80] = "n:/netbsd";

static struct kparamb kparam;

void help  PROTO((void));
void usage PROTO((void));
void get_sys_info PROTO((void));
void start_kernel PROTO((void));

int
main(argc, argv)
int	argc;
char	**argv;
{
	/*
	 * Option parsing
	 */
	extern	int	optind;
	extern	char	*optarg;
	int		ch;
	int		fd;
	long		textsz, stringsz;
	struct exec	ehdr;
	
	init_toslib(argv[0]);
	Progname = argv[0];

	kparam.boothowto = RB_SINGLE;

	while ((ch = getopt(argc, argv, "abdhstVwDo:S:T:")) != EOF) {
		switch (ch) {
		case 'a':
			kparam.boothowto &= ~(RB_SINGLE);
			kparam.boothowto |= RB_AUTOBOOT;
			break;
		case 'b':
			kparam.boothowto |= RB_ASKNAME;
			break;
		case 'd':
			kparam.boothowto |= RB_KDB;
			break;
		case 'D':
			d_flag = 1;
			break;
		case 'h':
			h_flag = 1;
			break;
		case 'o':
			redirect_output(optarg);
			break;
		case 's':
			s_flag = 1;
			break;
		case 'S':
			kparam.stmem_size = atoi(optarg);
			break;
		case 't':
			t_flag = 1;
			break;
		case 'T':
			kparam.ttmem_size = atoi(optarg);
			break;
		case 'V':
			v_flag = 1;
			break;
		case 'w':
			set_wait_for_key();
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc == 1)
		strcpy(kname, argv[0]);

	if (h_flag)
		help();
	if (v_flag)
		eprintf("%s\r\n", version);

	/*
	 * Get system info to pass to NetBSD
	 */
	get_sys_info();

	/*
	 * Find the kernel to boot and read it's exec-header
	 */
	if ((fd = open(kname, O_RDONLY)) < 0)
		fatal(-1, "Cannot open kernel '%s'", kname);
	if (read(fd, (char *)&ehdr, sizeof(ehdr)) != sizeof(ehdr))
		fatal(-1, "Cannot read exec-header of '%s'", kname);
	if (N_MAGIC(ehdr) != NMAGIC)
		fatal(-1, "Not an NMAGIC file '%s'", kname);

	/*
	 * Extract various sizes from the kernel executable
	 */
	textsz          = (ehdr.a_text + __LDPGSZ - 1) & ~(__LDPGSZ - 1);
	kparam.esym_loc = 0;
	kparam.ksize    = textsz + ehdr.a_data + ehdr.a_bss;
	kparam.entry    = ehdr.a_entry;

	if (ehdr.a_syms) {
	  if (lseek(fd,ehdr.a_text+ehdr.a_data+ehdr.a_syms+sizeof(ehdr),0) <= 0)
		fatal(-1, "Cannot seek to string table in '%s'", kname);
	  if (read(fd, (char *)&stringsz, sizeof(long)) != sizeof(long))
		fatal(-1, "Cannot read string-table size");
	  if (lseek(fd, sizeof(ehdr), 0) <= 0)
		fatal(-1, "Cannot seek back to text start");
	  kparam.ksize += ehdr.a_syms + sizeof(long) + stringsz;
	}

	if ((kparam.kp = (u_char *)malloc(kparam.ksize)) == NULL)
		fatal(-1, "Cannot malloc kernel image space");

	/*
	 * Read text & data, clear bss
	 */
	if ((read(fd, (char *)kparam.kp, ehdr.a_text) != ehdr.a_text)
	    || (read(fd,(char *)(kparam.kp+textsz),ehdr.a_data) != ehdr.a_data))
		fatal(-1, "Unable to read kernel image\n");
	memset(kparam.kp + textsz + ehdr.a_data, 0, ehdr.a_bss);

	/*
	 * Read symbol and string table
	 */
	if (ehdr.a_syms) {
		long	*p;

		p = (long *)(kparam.kp + textsz + ehdr.a_data + ehdr.a_bss);
		*p++ = ehdr.a_syms;
		if (read(fd, (char *)p, ehdr.a_syms) != ehdr.a_syms)
			fatal(-1, "Cannot read symbol table\n");
		p = (long *)((char *)p + ehdr.a_syms);
		if (read(fd, (char *)p, stringsz) != stringsz)
			fatal(-1, "Cannot read string table\n");
		kparam.esym_loc = (long)((char *)p-(char *)kparam.kp +stringsz);
	}

	if (d_flag) {
	    eprintf("\r\nKernel info:\r\n");
	    eprintf("Kernel loadaddr\t: 0x%08x\r\n", kparam.kp);
	    eprintf("Kernel size\t: %10d bytes\r\n", kparam.ksize);
	    eprintf("Kernel entry\t: 0x%08x\r\n", kparam.entry);
	    eprintf("Kernel esym\t: 0x%08x\r\n", kparam.esym_loc);
	}

	if (!t_flag)
		start_kernel();
		/* NOT REACHED */

	eprintf("Kernel '%s' was loaded OK\r\n", kname);
	xexit(0);
}

/*
 * Extract memory and cpu/fpu info from system.
 */
void
get_sys_info()
{
	long	stck;
	long	*jar;
	OSH	*oshdr;

	kparam.bootflags = 0;

	stck = Super(0);

	/*
	 * Some GEMDOS versions use a different year-base in the RTC.
	 */
	oshdr = *ADDR_OSHEAD;
	oshdr = oshdr->os_beg;
	if ((oshdr->os_version > 0x0300) && (oshdr->os_version < 0x0306))
		kparam.bootflags |= ATARI_CLKBROKEN;

	if (kparam.stmem_size <= 0)
		kparam.stmem_size  = *ADDR_PHYSTOP;

	if (kparam.ttmem_size)
		kparam.ttmem_start  = TTRAM_BASE;
	else {
		if (!s_flag && (*ADDR_CHKRAMTOP == RAM_TOP_MAGIC)) {
			kparam.ttmem_size  = *ADDR_RAMTOP;
			if (kparam.ttmem_size > TTRAM_BASE) {
				kparam.ttmem_size  -= TTRAM_BASE;
				kparam.ttmem_start  = TTRAM_BASE;
			}
			else kparam.ttmem_size = 0;
		}
	}

	/*
	 * Scan cookiejar for cpu types
	 */
	jar = *ADDR_P_COOKIE;
	if (jar != NULL) {
		do {
			if (jar[0] == 0x5f435055) { /* _CPU	*/
				switch (jar[1]) {
					case 0:
						kparam.bootflags |= ATARI_68000;
						break;
					case 10:
						kparam.bootflags |= ATARI_68010;
						break;
					case 20:
						kparam.bootflags |= ATARI_68020;
						break;
					case 30:
						kparam.bootflags |= ATARI_68030;
						break;
					case 40:
						kparam.bootflags |= ATARI_68040;
						break;
					default:
						fatal(-1, "Unknown CPU-type");
				}
			}
			if (jar[0] == 0x42504658) { /* BPFX	*/
				unsigned long	*p;

				p = (unsigned long*)jar[1];

				kparam.ttmem_start = p[1];
				kparam.ttmem_size  = p[2];
			}
			jar = &jar[2];
		} while (jar[-2]);
	}
	if (!(kparam.bootflags & ATARI_ANYCPU))
		fatal(-1, "Cannot determine CPU-type");

	(void)Super(stck);

	if (d_flag) {
	    eprintf("Machine info:\r\n");
	    eprintf("ST-RAM size\t: %10d bytes\r\n",kparam.stmem_size);
	    eprintf("TT-RAM size\t: %10d bytes\r\n",kparam.ttmem_size);
	    eprintf("TT-RAM start\t: 0x%08x\r\n", kparam.ttmem_start);
	    eprintf("Cpu-type\t: 0x%08x\r\n", kparam.bootflags);
	}
}

void
help()
{
	eprintf("\r
NetBSD loader for the Atari-TT\r
\r
Usage: %s [-abdhstVD] [-S <stram-size>] [-T <ttram-size>] [kernel]\r
\r
Description of options:\r
\r
\t-a  Boot up to multi-user mode.\r
\t-b  Ask for root device to use.\r
\t-d  Enter kernel debugger.\r
\t-D  printout debug information while loading\r
\t-h  What you're getting right now.\r
\t-o  Write output to both <output file> and stdout.\r
\t-s  Use only ST-compatible RAM\r
\t-S  Set amount of ST-compatible RAM\r
\t-T  Set amount of TT-compatible RAM\r
\t-t  Test the loader. It will do everything except executing the\r
\t    loaded kernel.\r
\t-V  Print loader version.\r
\t-w  Wait for a keypress before exiting.\r
", Progname);
	xexit(0);
}

void
usage()
{
	eprintf("Usage: %s [-abdhstVD] [-S <stram-size>] "
		"[-T <ttram-size>] [kernel]\r\n", Progname);
	xexit(1);
}

void
start_kernel()
{
	long	stck;

	stck = Super(0);
	bsd_startup(&kparam);
	/* NOT REACHED */

	(void)Super(stck);
}
