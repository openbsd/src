/* $OpenBSD: bsdbooter.c,v 1.2 2000/03/03 00:54:47 todd Exp $ */
/* $NetBSD: bsdbooter.c,v 1.1 1996/01/31 23:18:08 mark Exp $ */

/*
 * Copyright (c) 1994,1995 Mark Brinicombe.
 * Copyright (c) 1994,1995 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Mark Brinicombe.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * bsdbooter.c
 *
 * RiscBSD boot loader
 *
 * Created      : 12/09/94
 * Last updated : 08/03/95
 *
 * Based on kate/boot/boot.c
 *
 */

/* Include standard header files */

# include <stdlib.h>
# include <string.h>
# include <stdarg.h>

/* Include local headers */

#include <sys/types.h>
#include <sys/exec.h>
#include <machine/cpu.h>
#include <machine/katelib.h>
#include <machine/bootconfig.h>
#include "swiv.h"
#include "swis.h"

/*
 * Declare global variables
 */

# define VERSION "2.10"

# define USE_MODULEAREA

# define KERNAREA 512
# define KERNBASE 0xf0000000

# define TABLEAREA 513
# define LOADAREA 514

# define SCRATCHSIZE 0xc000


# define OS_DynamicArea 0x66
# define OS_Memory      0x68
# define OS_MMUControl  0x6b

# define FASTBOOT_FILENAME "<BtRiscBSD$Dir>.booter.fastboot"

/*
 * Declare external variables
 */

extern char *__cli;

/*
 * Local vars. Some of these have to be global because they are used after
 * the processor has been switches to SVC mode local variables would be
 * lost as they would be on the USR mode stack.
 */

typedef struct exec aout_t;

BootConfig bootconfig;
int in[3], out[3];
char kernelname[1024];
unsigned char *buffer;
static aout_t aout;
unsigned int kernelsize;
unsigned int logical;
unsigned int physical;
unsigned int filesize;
unsigned int copysize;

/*
 * Local function prototypes
 */

void fatal(struct Error *error);
unsigned char *locate_memory_blocks(void);
void uprintf(char *formattoken, ...);
void _bsdboot(BootConfig *bootconfig, unsigned int address);
int vsprintf(char *buf, const char *fmt, va_list args);

/* Now for the main code */

extern int main (int, char **);

void __exit(int);

void _main(void)
  {
	uprintf("_main entered\n");
    __exit (main (0, (char **)0));	/* ... ignition */

/* not reached */
  }

/* The main booter code */

int main(int argc, char *argv[])
  {
    char *cliptr;
    int loop;
    int filehandle;
    unsigned char *arrangementtable;

	uprintf("main entered\n");

/* Analyse the command line */

    cliptr = __cli;

	uprintf("command line is %s\n", cliptr);
	
/* Skip the command name */

    while (*cliptr != ' ' && *cliptr != 0)
      ++cliptr;

/* Skip any spaces */

    while (*cliptr == ' ')
      ++cliptr;

/* Check for another parameter */

    if (*cliptr != 0)
      {
        for (loop = 0; *cliptr != ' ' && *cliptr != 0; ++loop,++cliptr)
          {
            kernelname[loop] = *cliptr;
          }
        kernelname[loop] = 0;
      }
    else
      strcpy(kernelname, "riscbsd");

    strcpy(bootconfig.kernelname, kernelname);

/* Write the command line used to a fastboot file. Execing or Obeying
 * this file will boot RiscBSD. This can be used during the RiscOS bootup
 * to enable a fast boot.
 */

/*
 * Open the autoboot file. Just skip if file cannot be opened.
 */

    swi(OS_Find, IN(R0|R1)|OUT(R0), 0x80, FASTBOOT_FILENAME, &filehandle);
    if (filehandle != 0)
      {
        swi(OS_GBPB, IN(R0|R1|R2|R3|R4), 2, filehandle, __cli,
          strlen(__cli), 0);
        swi(OS_GBPB, IN(R0|R1|R2|R3|R4), 2, filehandle, "\n", 1, 0);

/* Close the file */

        swi(OS_Find, IN(R0|R1), 0, filehandle);

        swi(OS_File, IN(R0|R1|R2), 18, FASTBOOT_FILENAME, 0xfeb);
      }
    else
      {
        uprintf("Warning: Cannot write fastboot file %s\n\r", FASTBOOT_FILENAME);
      }

/* Set the screen mode ... */

/* I know this is messy. It is currently just a hack to try things out
 * Why didn't Acorn add a SWI call to interpret the mode string and return
 * a mode specifer ?
 */

/* Also this is temporary as eventually the console driver will set the VIDC
 * up as required.
 * It sort of expects the screenmode= options to be at the end of the string.
 */

    {
      char *modeptr;
      int modespec[6];

      modeptr = strstr(__cli, "screenmode=");
      if (modeptr)
        {
          modeptr += 11;
          modespec[0] = 0x00000001;
          modespec[1] = 0x00000000;
          modespec[2] = 0x00000000;
          modespec[3] = 0x00000003;
          modespec[4] = 0x00000000;
          modespec[5] = -1;

          while (*modeptr)
            {
              switch (*modeptr)
                {
                  case 'X':
                  case 'x':
                    ++modeptr;
                    while (*modeptr >= '0' && *modeptr <= '9')
                      {
                        modespec[1] = (modespec[1] * 10) + (*modeptr - '0');
                        ++modeptr;
                      }
                    break;

                  case 'Y':
                  case 'y':
                    ++modeptr;
                    while (*modeptr >= '0' && *modeptr <= '9')
                      {
                        modespec[2] = (modespec[2] * 10) + (*modeptr - '0');
                        ++modeptr;
                      }
                    break;
/*
                  case 'C':
                  case 'c':
                  case 'G':
                  case 'g':
                    ++modeptr;
                    while (*modeptr >= '0' && *modeptr <= '9')
                      {
                        modespec[3] = (modespec[3] * 10) + (*modeptr - '0');
                        ++modeptr;
                      }
                    break;
*/
                  case 'F':
                  case 'f':
                    ++modeptr;
                    while (*modeptr >= '0' && *modeptr <= '9')
                      {
                        modespec[4] = (modespec[4] * 10) + (*modeptr - '0');
                        ++modeptr;
                      }
                    break;

                  default:
                    ++modeptr;
                    break;
                }
            }
          if (modespec[4] == 0) modespec[4] = -1;
/*          uprintf("x=%d y=%d c=%d f=%d\n", modespec[1], modespec[2],
            modespec[3], modespec[4]);*/
          fatal(swix(Wimp_SetMode, IN(R0), &modespec));
          bootconfig.framerate = modespec[4];
        }
      else
        bootconfig.framerate = 0;
    }

/* Announcement time .. */
/* Used to be above but now moved to after the mode change */


    if (strstr(__cli, "verbose") != 0)
      uprintf("RiscBSD BootLoader " VERSION " " __DATE__ "\n\r");

/* A bit of info */

    if (strstr(__cli, "verbose") != 0)
      uprintf("Kernel: %s\n\r", kernelname);

/* Get the machine id */

    fatal(swix(OS_ReadSysInfo, IN(R0)|OUT(R3), 2, &bootconfig.machine_id));

/* Get the display variables. Failure on any of these will abort the boot */

    in[0] = 149;
    in[1] = 150;
    in[2] = -1;

    fatal(swix(OS_ReadVduVariables, IN(R0|R1), &in, &out));

    bootconfig.display_start = out[0];
    bootconfig.display_size = out[1];

    fatal(swix(OS_ReadModeVariable, IN(R0|R1) | OUT(R2), -1, 9,
      &bootconfig.bitsperpixel));
    fatal(swix(OS_ReadModeVariable, IN(R0|R1) | OUT(R2), -1, 11,
      &bootconfig.width));
    fatal(swix(OS_ReadModeVariable, IN(R0|R1) | OUT(R2), -1, 12,
      &bootconfig.height));

/* Will the kernel support this mode ? */

    if (bootconfig.bitsperpixel > 3)
      {
        swi(OS_Write0, IN(R0),
          "Error: Only 1, 2, 4 or 8 bpp modes are currently supported\n\r");
        return(0);
      }

/* Get the arrangement table for the memory */

    arrangementtable = locate_memory_blocks();

/*
 * Ok we will support a.out files as well. This means that we need to
 * identify the format.
 */

/* Get the size of the file */

    fatal(swix(OS_File, IN(R0|R1)|OUT(R4), 5, kernelname, &filesize));

/*
 * Read the start of the file so that we change check for the a.out
 * magic number.
 */

    swi(OS_Find, IN(R0|R1)|OUT(R0), 0x40, kernelname, &filehandle);
    if (filehandle == 0)
      {
        uprintf("Error: Cannot read kernel file %s\n\r", kernelname);
        return(0);
      }

    aout.a_midmag = 0;

    fatal(swix(OS_GBPB, IN(R0|R1|R2|R3|R4), 3, filehandle, &aout,
      sizeof(aout_t), 0));

/* Do we have an a.out file ? */

    switch(N_GETMAGIC(aout)) {
      case NMAGIC:
        if (strstr(__cli, "verbose") != 0)
          swi(OS_Write0, IN(R0), "Kernel binary is NMAGIC a.out format\n\r");
        kernelsize = (unsigned int)(aout.a_text + aout.a_data + aout.a_bss);
        copysize = (unsigned int)(aout.a_text + aout.a_data);
	break;
      case OMAGIC:
        if (strstr(__cli, "verbose") != 0)
          swi(OS_Write0, IN(R0), "Kernel binary is OMAGIC a.out format\n\r");
        kernelsize = (unsigned int)(aout.a_text + aout.a_data + aout.a_bss);
        copysize = (unsigned int)(aout.a_text + aout.a_data);
	break;
      case ZMAGIC:
        if (strstr(__cli, "verbose") != 0)
          swi(OS_Write0, IN(R0), "Kernel binary is ZMAGIC a.out format\n\r");
        kernelsize = (unsigned int)(aout.a_text + aout.a_data + aout.a_bss);
        copysize = (unsigned int)(aout.a_text + aout.a_data);
	break;
      default:
        if (strstr(__cli, "verbose") != 0)
          swi(OS_Write0, IN(R0), "Kernel binary is AIF format\n\r");
        kernelsize = filesize;
        copysize = filesize;
        break;
    }

/* Give ourselves 16K of spare space and round off to a page */

/*
 * This is messy. We should read the memory info first, but I have not
 * changed things yet. This is part of the hack to support a.out files
 * as well
 */

    kernelsize = (kernelsize + 0x4000) & ~(bootconfig.pagesize-1);

/* Set the virtual address of the kernel in the bootconfig structure */

    bootconfig.kernvirtualbase = KERNBASE;
    bootconfig.kernsize = kernelsize;
    bootconfig.argvirtualbase = bootconfig.kernvirtualbase
                              + bootconfig.kernsize;
    bootconfig.argsize = bootconfig.pagesize;
    bootconfig.scratchvirtualbase = bootconfig.argvirtualbase
                                  + bootconfig.argsize;
    bootconfig.scratchsize = SCRATCHSIZE;

    kernelsize += bootconfig.argsize;

    kernelsize += bootconfig.scratchsize;

/* Verbose info to the user. This is mainly debugging */

    if (strstr(__cli, "verbose") != 0)
      {
        uprintf("filesize = %08x\n\r", filesize);
        uprintf("bootconfig.kernvirtualbase = %08x\n\r",
          bootconfig.kernvirtualbase);
        uprintf("bootconfig.kernsize = %08x\n\r", bootconfig.kernsize);
        uprintf("bootconfig.argvirtualbase = %08x\n\r",
          bootconfig.argvirtualbase);
        uprintf("bootconfig.argsize = %08x\n\r", bootconfig.argsize);
        uprintf("bootconfig.scratchvirtualbase = %08x\n\r",
          bootconfig.scratchvirtualbase);
        uprintf("bootconfig.scratchsize = %08x\n\r", bootconfig.scratchsize);
        uprintf("kernelsize = %08x\n\r", kernelsize);
        uprintf("copysize = %08x\n\r", copysize);
      }

# ifdef USE_MODULEAREA

/* Allocate memory in module area to hold the data we are loading */

    fatal(swix(OS_Module, IN(R0|R3)|OUT(R2), 6, filesize, &buffer));

# else

/* Allocate memory to hold the data we are loading */

    swix(OS_DynamicArea, IN(R0|R1), 1, LOADAREA);

    fatal(swix(OS_DynamicArea, IN(R0|R1|R2|R3|R4|R5|R6|R7|R8)|OUT(R3), 0,
      LOADAREA, filesize, -1, 0x80, filesize, 0, 0, "Kate Data", &buffer));

# endif

/* Load the appropriate part depending on the file type */

    switch (N_GETMAGIC(aout)) {
      case OMAGIC:
      case NMAGIC:
        swi(OS_GBPB, IN(R0|R1|R2|R3|R4), 3, filehandle, buffer,
          filesize, sizeof(aout_t));
	break;
      case ZMAGIC:
      default:
        swi(OS_GBPB, IN(R0|R1|R2|R3|R4), 3, filehandle, buffer,
          filesize, 0);
        break;
    }

/* Close the file */

    fatal(swix(OS_Find, IN(R0|R1), 0, filehandle));


/* This is redundant at the moment */

    swix(OS_DynamicArea, IN(R0|R1), 1, KERNAREA);

    fatal(swix(OS_DynamicArea, IN(R0|R1|R2|R3|R4|R5|R6|R7|R8), 0,
      KERNAREA, 0, KERNBASE, 0x80, 0x1000, 0, 0, "Kate Kernel"));

/* Shutdown RiscOS cleanly ... */

/* Close all open files and shutdown filing systems */

    swix(OS_FSControl, IN(R0), 23);

/* Issue a pre-reset service call to reset the podules */

    swix(OS_ServiceCall, IN(R1), 0x45);

/* Kill the etherH module to avoid locks up on reboot */

    swix(OS_Module, IN(R0|R1), 4, "EtherH");

/* More user information describing the memory found */

    if (strstr(__cli, "verbose") != 0)
      {
        uprintf("DRAM bank 0a = %08x %08x\n\r", bootconfig.dram[0].address,
          bootconfig.dram[0].pages * bootconfig.pagesize);
        uprintf("DRAM bank 0b = %08x %08x\n\r", bootconfig.dram[1].address,
          bootconfig.dram[1].pages * bootconfig.pagesize);
        uprintf("DRAM bank 1a = %08x %08x\n\r", bootconfig.dram[2].address,
          bootconfig.dram[2].pages * bootconfig.pagesize);
        uprintf("DRAM bank 1b = %08x %08x\n\r", bootconfig.dram[3].address,
          bootconfig.dram[3].pages * bootconfig.pagesize);
        uprintf("VRAM bank 0  = %08x %08x\n\r", bootconfig.vram[0].address,
          bootconfig.vram[0].pages * bootconfig.pagesize);
    }

/* Hack for 2 Meg VRAM until the new console code is in place */

/*    if (strstr(__cli, "vramhack") != 0)
      {
        bootconfig.display_size /= 1;
        bootconfig.vram[0].pages /= 2;

        uprintf("VRAM bank 0  = %08x %08x\n\r", bootconfig.vram[0].address,
          bootconfig.vram[0].pages * bootconfig.pagesize);
      }*/

/* Jump to SVC26 mode - remember we have no local vars now ! */

    EnterOS();

/* Find the number of the upper most bank of DRAM available */

    loop = 3;
    while (bootconfig.dram[loop].address == 0)
      --loop;

/* Allocate the physical addresses for the kernel in this bank */

    physical = bootconfig.dram[loop].address - kernelsize
             + bootconfig.dram[loop].pages * bootconfig.pagesize;
    bootconfig.kernphysicalbase = physical;
    bootconfig.argphysicalbase = bootconfig.kernphysicalbase
                               + bootconfig.kernsize;
    bootconfig.scratchphysicalbase = bootconfig.argphysicalbase
                                   + bootconfig.argsize;

/* Yet more debugging info */

    if (strstr(__cli, "verbose") != 0)
      {
        uprintf("buffer = %08x\n\r", buffer);
        uprintf("physical = %08x\n\r", physical);
        uprintf("bootconfig.kernphysicalbase = %08x\n\r",
          bootconfig.kernphysicalbase);
        uprintf("bootconfig.argphysicalbase = %08x\n\r",
          bootconfig.argphysicalbase);
        uprintf("bootconfig.scratchphysicalbase = %08x\n\r",
          bootconfig.scratchphysicalbase);
      }

/*
 * Ok just check to see if anything is mapped where we are about to map
 * the kernel.
 */

/*
    for (logical = KERNBASE; logical < KERNBASE + kernelsize;
     logical += bootconfig.pagesize)
      {
        if (ReadWord(0x02c00000 + (logical >> 10) & 0xfffffffc) != 0)
          {
            uprintf("Error: Memory required for RiscBSD boot not available\n\r");
            return(0);
          }
      }
*/
/* Get out clause */

    if (strstr(__cli, "noboot") != 0)
      {
        ExitOS();
        return(0);
      }

/*
 * Hook the physical pages to the required virtual address directly by
 * writing into RiscOS's page tables. This should be done via a
 * dynamic area handler but I cannot get it to work as documented.
 */

    for (logical = KERNBASE; logical < KERNBASE + kernelsize;
     logical += bootconfig.pagesize)
      {
        WriteWord(0x02c00000 + (logical >> 10) & 0xfffffffc,
          0x00000ffe | (physical & 0xfffff000));
        physical += bootconfig.pagesize;
      }

/* Map the IO up high so we can get at it */

    WriteWord(0x02c0c000 + (0xf6000000 >> 18) & 0xfffffffc,
      0x00000412 | (0x03200000 & 0xfffff000));
    WriteWord(0x02c0c000 + (0xf6100000 >> 18) & 0xfffffffc,
      0x00000412 | (0x03400000 & 0xfffff000));

    memset((char *)bootconfig.display_start, 0xcc, 0x4000);

/* Disable IRQ and FIQ interrupts */

    SetCPSR(I32_bit | F32_bit, I32_bit | F32_bit);

    memset((char *)bootconfig.display_start + 0x4000, 0x55, 0x4000);

    memcpy((char *)bootconfig.argvirtualbase, __cli, bootconfig.argsize);

    memset((char *)bootconfig.display_start + 0x8000, 0x80, 0x4000);

    memset((char *)bootconfig.argvirtualbase, SCRATCHSIZE, 0);

    memset((char *)bootconfig.display_start + 0xC000, 0xbb, 0x4000);

    memcpy((char *)bootconfig.kernvirtualbase, buffer, copysize);

    memset((char *)bootconfig.display_start + 0x10000, 0xaa, 0x4000);

/* Real late debugging get out clause */

    if (strstr(__cli, "nearboot") != 0)
      {
        SetCPSR(I32_bit | F32_bit, 0);
        ExitOS();
        return(0);
      }

/* Punch into SVC32 mode */

    SVC32();

/* Point of no return */

    switch (N_GETMAGIC(aout)) {
      case OMAGIC:
      case NMAGIC:
      case ZMAGIC:
        _bsdboot(&bootconfig, (unsigned int)aout.a_entry);
        break;
      default:
        _bsdboot(&bootconfig, KERNBASE);
        break;
    }

    return(0);
  }


/* Report an error */

void fatal(struct Error *error)
  {
    if (error)
      {
        swi(OS_GenerateError, IN(R0), error);
      }
  }


/* Locate all the blocks of memory in the system */

unsigned char *locate_memory_blocks(void)
  {
    int loop;
    int page;
    int currentpage;
    int currentpages;
    int currentaddr;
    unsigned char *table;
    unsigned int pagesize;
    unsigned int tablesize;
    int dramblocks = 0;
    int vramblocks = 0;

/* Get table size and page size */

    fatal(swix(OS_Memory, IN(R0)|OUT(R1|R2), 6, &tablesize, &pagesize));

/* Allocate memory for table */

/*# ifdef USE_MODULEAREA*/

    fatal(swix(OS_Module, IN(R0|R3)|OUT(R2), 6, tablesize, &table));

/*# else*/

/* Allocate memory to hold the data we are loading */

/*    swix(OS_DynamicArea, IN(R0|R1), 1, TABLEAREA);

    fatal(swix(OS_DynamicArea, IN(R0|R1|R2|R3|R4|R5|R6|R7|R8)|OUT(R3), 0,
      TABLEAREA, tablesize, -1, 0x80, tablesize, 0, 0, "Kate Table", &table));

# endif*/


/* read the table */

    fatal(swix(OS_Memory, IN(R0|R1), 7, table));

/* Loop round locating all the valid blocks of memory */

    currentpage = -1;

    for (loop = 0; loop < tablesize * 2; ++loop)
      {
        page = table[loop / 2];
        if (loop % 2)
          page = page >> 4;

        page = page & 0x07;

        if (page != currentpage)
          {
            switch (currentpage)
              {
                case 1:
                  bootconfig.dram[dramblocks].address = currentaddr * pagesize;
                  bootconfig.dram[dramblocks].pages = currentpages;
                  ++dramblocks;
                  break;

                case 2:
                  bootconfig.vram[vramblocks].address = currentaddr * pagesize;
                  bootconfig.vram[vramblocks].pages = currentpages;
                  ++vramblocks;
                  break;

                default :
                  break;
              }

            currentpage = page;
            currentaddr = loop;
            currentpages = 0;
          }
        ++currentpages;
      }

/* Get the number of dram and vram pages */

    fatal(swix(OS_Memory, IN(R0)|OUT(R1), 0x00000108, &bootconfig.drampages));
    fatal(swix(OS_Memory, IN(R0)|OUT(R1), 0x00000208, &bootconfig.vrampages));

/* Fill in more bootconfig parameters */

    bootconfig.pagesize = pagesize;

    bootconfig.dramblocks = dramblocks;
    bootconfig.vramblocks = vramblocks;

    return(table);
  }


/* printf ... */

void uprintf(char *formattoken, ...)
  {
    va_list ap;
    char temp[1024];

    temp[0] = '\0';

    va_start(ap, formattoken);
    vsprintf(temp, formattoken, ap);
    va_end(ap);

    swi(OS_Write0, IN(R0), temp);
  }

/* End of bsdbooter.c */
