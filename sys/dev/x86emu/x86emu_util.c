/*	$NetBSD: x86emu_util.c,v 1.2 2007/12/04 17:32:22 joerg Exp $	*/
/* $OpenBSD */

/****************************************************************************
*
*  Realmode X86 Emulator Library
*
*  Copyright (C) 1996-1999 SciTech Software, Inc.
*  Copyright (C) David Mosberger-Tang
*  Copyright (C) 1999 Egbert Eich
*  Copyright (C) 2007 Joerg Sonnenberger
*
*  ========================================================================
*
*  Permission to use, copy, modify, distribute, and sell this software and
*  its documentation for any purpose is hereby granted without fee,
*  provided that the above copyright notice appear in all copies and that
*  both that copyright notice and this permission notice appear in
*  supporting documentation, and that the name of the authors not be used
*  in advertising or publicity pertaining to distribution of the software
*  without specific, written prior permission.  The authors makes no
*  representations about the suitability of this software for any purpose.
*  It is provided "as is" without express or implied warranty.
*
*  THE AUTHORS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
*  INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
*  EVENT SHALL THE AUTHORS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
*  CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
*  USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
*  OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
*  PERFORMANCE OF THIS SOFTWARE.
*
****************************************************************************/

#include <sys/param.h>
#include <sys/endian.h>

#include <dev/x86emu/x86emu.h>
#include <dev/x86emu/x86emu_regs.h>


/****************************************************************************
PARAMETERS:
addr	- Emulator memory address to read

RETURNS:
Byte value read from emulator memory.

REMARKS:
Reads a byte value from the emulator memory.
****************************************************************************/
static uint8_t
rdb(struct X86EMU *emu, uint32_t addr)
{
	if (addr > emu->mem_size - 1)
		X86EMU_halt_sys(emu);
	return emu->mem_base[addr];
}
/****************************************************************************
PARAMETERS:
addr	- Emulator memory address to read

RETURNS:
Word value read from emulator memory.

REMARKS:
Reads a word value from the emulator memory.
****************************************************************************/
static uint16_t
rdw(struct X86EMU *emu, uint32_t addr)
{
	if (addr > emu->mem_size - 2)
		X86EMU_halt_sys(emu);
	/* XXX alignment *sigh* */
	return *(u_int16_t *)(emu->mem_base + addr);
}
/****************************************************************************
PARAMETERS:
addr	- Emulator memory address to read

RETURNS:
Long value read from emulator memory.
REMARKS:
Reads a long value from the emulator memory.
****************************************************************************/
static uint32_t
rdl(struct X86EMU *emu, uint32_t addr)
{
	if (addr > emu->mem_size - 4)
		X86EMU_halt_sys(emu);
	/* XXX alignment *sigh* */
	return *(u_int32_t *)(emu->mem_base + addr);
}
/****************************************************************************
PARAMETERS:
addr	- Emulator memory address to read
val		- Value to store

REMARKS:
Writes a byte value to emulator memory.
****************************************************************************/
static void
wrb(struct X86EMU *emu, uint32_t addr, uint8_t val)
{
	if (addr > emu->mem_size - 1)
		X86EMU_halt_sys(emu);
	emu->mem_base[addr] = val;
}
/****************************************************************************
PARAMETERS:
addr	- Emulator memory address to read
val		- Value to store

REMARKS:
Writes a word value to emulator memory.
****************************************************************************/
static void
wrw(struct X86EMU *emu, uint32_t addr, uint16_t val)
{
	if (addr > emu->mem_size - 2)
		X86EMU_halt_sys(emu);
	/* XXX alignment */
	*((u_int16_t *)(emu->mem_base + addr)) = val;
}
/****************************************************************************
PARAMETERS:
addr	- Emulator memory address to read
val		- Value to store

REMARKS:
Writes a long value to emulator memory.
****************************************************************************/
static void
wrl(struct X86EMU *emu, uint32_t addr, uint32_t val)
{
	if (addr > emu->mem_size - 4)
		X86EMU_halt_sys(emu);
	/* XXX alignment *sigh* */
	*((u_int32_t *)(emu->mem_base + addr)) = val;
}

/*----------------------------- Setup -------------------------------------*/

void
X86EMU_init_default(struct X86EMU *emu)
{
	int i;

	emu->emu_rdb = rdb;
	emu->emu_rdw = rdw;
	emu->emu_rdl = rdl;
	emu->emu_wrb = wrb;
	emu->emu_wrw = wrw;
	emu->emu_wrl = wrl;

	for (i = 0; i < 256; i++)
		emu->_X86EMU_intrTab[i] = NULL;
}
