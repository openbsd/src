
/* $OpenBSD: exec_i386.c,v 1.1 1997/03/31 03:12:13 weingart Exp $ */

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/reboot.h>
#include <libsa.h>


void
machdep_start(startaddr, howto, loadaddr, ssym, esym)
	char *startaddr, *loadaddr, *ssym, *esym;
	int howto;
{
	static int argv[9];
	static int (*x_entry)() = 0;

	x_entry = (void *)startaddr;
	(int)startaddr &= 0xffffff;

	/*
	 *  We now pass the various bootstrap parameters to the loaded
	 *  image via the argument list
	 *
	 *  arg0 = 8 (magic)
	 *  arg1 = boot flags
	 *  arg2 = boot device
	 *  arg3 = start of symbol table (0 if not loaded)
	 *  arg4 = end of symbol table (0 if not loaded)
	 *  arg5 = transfer address from image
	 *  arg6 = transfer address for next image pointer
	 *  arg7 = conventional memory size (640)
	 *  arg8 = extended memory size (8196)
	 */
	argv[0] = 8;
	argv[1] = howto;
	argv[2] = 0;		/* Boot device */
	argv[3] = 0;		/* Cyl offset */
	argv[4] = (int)esym;
	argv[5] = (int)startaddr;
	argv[6] = (int)&x_entry;
	argv[7] = 0;
	argv[8] = 0;

	/****************************************************************/
	/* copy that first page and overwrite any BIOS variables        */
	/****************************************************************/
	printf("entry point at 0x%x\n", (int)startaddr);
	startprog(startaddr, argv);
}

