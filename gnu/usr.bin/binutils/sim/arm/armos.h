/*  armos.h -- ARMulator OS definitions:  ARM6 Instruction Emulator.
    Copyright (C) 1994 Advanced RISC Machines Ltd.
 
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
 
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

/***************************************************************************\
*                   Define the initial layout of memory                     *
\***************************************************************************/

#define ADDRSUPERSTACK          0x800L   /* supervisor stack space */
#define ADDRUSERSTACK           0x80000L /* default user stack start */
#define ADDRSOFTVECTORS         0x840L   /* soft vectors are here */
#define ADDRCMDLINE             0xf00L   /* command line is here after a SWI GetEnv */
#define ADDRSOFHANDLERS         0xad0L   /* address and workspace for installed handlers */
#define SOFTVECTORCODE          0xb80L   /* default handlers */

/***************************************************************************\
*                               SWI numbers                                 *
\***************************************************************************/

#define SWI_WriteC                 0x0
#define SWI_Write0                 0x2
#define SWI_ReadC                  0x4
#define SWI_CLI                    0x5
#define SWI_GetEnv                 0x10
#define SWI_Exit                   0x11
#define SWI_EnterOS                0x16

#define SWI_GetErrno               0x60
#define SWI_Clock                  0x61
#define SWI_Time                   0x63
#define SWI_Remove                 0x64
#define SWI_Rename                 0x65
#define SWI_Open                   0x66

#define SWI_Close                  0x68
#define SWI_Write                  0x69
#define SWI_Read                   0x6a
#define SWI_Seek                   0x6b
#define SWI_Flen                   0x6c

#define SWI_IsTTY                  0x6e
#define SWI_TmpNam                 0x6f
#define SWI_InstallHandler         0x70
#define SWI_GenerateError          0x71

#define SWI_Breakpoint             0x180000 /* see gdb's tm-arm.h */

#define FPESTART 0x2000L
#define FPEEND 0x8000L
#define FPEOLDVECT FPESTART + 0x100L + 8L * 16L + 4L /* stack + 8 regs + fpsr */
#define FPENEWVECT(addr) 0xea000000L + ((addr) >> 2) - 3L /* branch from 4 to 0x2400 */
extern unsigned long fpecode[] ;
extern unsigned long fpesize ;
