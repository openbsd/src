/* Opcode table for the ARM.

   Copyright 1994, 1995 Free Software Foundation, Inc.
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */


struct arm_opcode {
    unsigned long value, mask;	/* recognise instruction if (op&mask)==value */
    char *assembler;		/* how to disassemble this instruction */
};

/* format of the assembler string :
   
   %%			%
   %<bitfield>d		print the bitfield in decimal
   %<bitfield>x		print the bitfield in hex
   %<bitfield>r		print as an ARM register
   %<bitfield>f		print a floating point constant if >7 else a
			floating point register
   %c			print condition code (always bits 28-31)
   %P			print floating point precision in arithmetic insn
   %Q			print floating point precision in ldf/stf insn
   %R			print floating point rounding mode
   %<bitnum>'c		print specified char iff bit is one
   %<bitnum>`c		print specified char iff bit is zero
   %<bitnum>?ab		print a if bit is one else print b
   %p			print 'p' iff bits 12-15 are 15
   %t			print 't' iff bit 21 set and bit 24 clear
   %o			print operand2 (immediate or register + shift)
   %a			print address for ldr/str instruction
   %b			print branch destination
   %A			print address for ldc/stc/ldf/stf instruction
   %m			print register mask for ldm/stm instruction
   %C			print the PSR sub type.
   %F			print the COUNT field of a LFM/SFM instruction.
*/

/* Note: There is a partial ordering in this table - it must be searched from
   the top to obtain a correct match. */

static struct arm_opcode arm_opcodes[] = {
    /* ARM instructions */
    {0x00000090, 0x0fe000f0, "mul%c%20's\t%16-19r, %0-3r, %8-11r"},
    {0x00200090, 0x0fe000f0, "mla%c%20's\t%16-19r, %0-3r, %8-11r, %12-15r"},
    {0x01000090, 0x0fb00ff0, "swp%c%22'b\t%12-15r, %0-3r, [%16-19r]"},
    {0x00800090, 0x0fa000f0, "%22?sumull%c%20's\t%12-15r, %16-19r, %0-3r, %8-11r"},
    {0x00a00090, 0x0fa000f0, "%22?sumlal%c%20's\t%12-15r, %16-19r, %0-3r, %8-11r"},
    {0x00000000, 0x0de00000, "and%c%20's\t%12-15r, %16-19r, %o"},
    {0x00200000, 0x0de00000, "eor%c%20's\t%12-15r, %16-19r, %o"},
    {0x00400000, 0x0de00000, "sub%c%20's\t%12-15r, %16-19r, %o"},
    {0x00600000, 0x0de00000, "rsb%c%20's\t%12-15r, %16-19r, %o"},
    {0x00800000, 0x0de00000, "add%c%20's\t%12-15r, %16-19r, %o"},
    {0x00a00000, 0x0de00000, "adc%c%20's\t%12-15r, %16-19r, %o"},
    {0x00c00000, 0x0de00000, "sbc%c%20's\t%12-15r, %16-19r, %o"},
    {0x00e00000, 0x0de00000, "rsc%c%20's\t%12-15r, %16-19r, %o"},
    {0x0120f000, 0x0db6f000, "msr%c\t%22?scpsr%C, %o"},
    {0x010f0000, 0x0fbf0fff, "mrs%c\t%12-15r, %22?scpsr"},
    {0x01000000, 0x0de00000, "tst%c%p\t%16-19r, %o"},
    {0x01200000, 0x0de00000, "teq%c%p\t%16-19r, %o"},
    {0x01400000, 0x0de00000, "cmp%c%p\t%16-19r, %o"},
    {0x01600000, 0x0de00000, "cmn%c%p\t%16-19r, %o"},
    {0x01800000, 0x0de00000, "orr%c%20's\t%12-15r, %16-19r, %o"},
    {0x01a00000, 0x0de00000, "mov%c%20's\t%12-15r, %o"},
    {0x01c00000, 0x0de00000, "bic%c%20's\t%12-15r, %16-19r, %o"},
    {0x01e00000, 0x0de00000, "mvn%c%20's\t%12-15r, %o"},
    {0x04000000, 0x0c100000, "str%c%22'b%t\t%12-15r, %a"},
    {0x04100000, 0x0c100000, "ldr%c%22'b%t\t%12-15r, %a"},
    {0x08000000, 0x0e100000, "stm%c%23?id%24?ba\t%16-19r%21'!, %m%22'^"},
    {0x08100000, 0x0e100000, "ldm%c%23?id%24?ba\t%16-19r%21'!, %m%22'^"},
    {0x0a000000, 0x0e000000, "b%24'l%c\t%b"},
    {0x0f000000, 0x0f000000, "swi%c\t%0-23x"},

    /* Floating point coprocessor instructions */
    {0x0e000100, 0x0ff08f10, "adf%c%P%R\t%12-14f, %16-18f, %0-3f"},
    {0x0e100100, 0x0ff08f10, "muf%c%P%R\t%12-14f, %16-18f, %0-3f"},
    {0x0e200100, 0x0ff08f10, "suf%c%P%R\t%12-14f, %16-18f, %0-3f"},
    {0x0e300100, 0x0ff08f10, "rsf%c%P%R\t%12-14f, %16-18f, %0-3f"},
    {0x0e400100, 0x0ff08f10, "dvf%c%P%R\t%12-14f, %16-18f, %0-3f"},
    {0x0e500100, 0x0ff08f10, "rdf%c%P%R\t%12-14f, %16-18f, %0-3f"},
    {0x0e600100, 0x0ff08f10, "pow%c%P%R\t%12-14f, %16-18f, %0-3f"},
    {0x0e700100, 0x0ff08f10, "rpw%c%P%R\t%12-14f, %16-18f, %0-3f"},
    {0x0e800100, 0x0ff08f10, "rmf%c%P%R\t%12-14f, %16-18f, %0-3f"},
    {0x0e900100, 0x0ff08f10, "fml%c%P%R\t%12-14f, %16-18f, %0-3f"},
    {0x0ea00100, 0x0ff08f10, "fdv%c%P%R\t%12-14f, %16-18f, %0-3f"},
    {0x0eb00100, 0x0ff08f10, "frd%c%P%R\t%12-14f, %16-18f, %0-3f"},
    {0x0ec00100, 0x0ff08f10, "pol%c%P%R\t%12-14f, %16-18f, %0-3f"},
    {0x0e008100, 0x0ff08f10, "mvf%c%P%R\t%12-14f, %0-3f"},
    {0x0e108100, 0x0ff08f10, "mnf%c%P%R\t%12-14f, %0-3f"},
    {0x0e208100, 0x0ff08f10, "abs%c%P%R\t%12-14f, %0-3f"},
    {0x0e308100, 0x0ff08f10, "rnd%c%P%R\t%12-14f, %0-3f"},
    {0x0e408100, 0x0ff08f10, "sqt%c%P%R\t%12-14f, %0-3f"},
    {0x0e508100, 0x0ff08f10, "log%c%P%R\t%12-14f, %0-3f"},
    {0x0e608100, 0x0ff08f10, "lgn%c%P%R\t%12-14f, %0-3f"},
    {0x0e708100, 0x0ff08f10, "exp%c%P%R\t%12-14f, %0-3f"},
    {0x0e808100, 0x0ff08f10, "sin%c%P%R\t%12-14f, %0-3f"},
    {0x0e908100, 0x0ff08f10, "cos%c%P%R\t%12-14f, %0-3f"},
    {0x0ea08100, 0x0ff08f10, "tan%c%P%R\t%12-14f, %0-3f"},
    {0x0eb08100, 0x0ff08f10, "asn%c%P%R\t%12-14f, %0-3f"},
    {0x0ec08100, 0x0ff08f10, "acs%c%P%R\t%12-14f, %0-3f"},
    {0x0ed08100, 0x0ff08f10, "atn%c%P%R\t%12-14f, %0-3f"},
    {0x0ee08100, 0x0ff08f10, "urd%c%P%R\t%12-14f, %0-3f"},
    {0x0ef08100, 0x0ff08f10, "nrm%c%P%R\t%12-14f, %0-3f"},
    {0x0e000110, 0x0ff00f1f, "flt%c%P%R\t%16-18f, %12-15r"},
    {0x0e100110, 0x0fff0f98, "fix%c%R\t%12-15r, %0-2f"},
    {0x0e200110, 0x0fff0fff, "wfs%c\t%12-15r"},
    {0x0e300110, 0x0fff0fff, "rfs%c\t%12-15r"},
    {0x0e400110, 0x0fff0fff, "wfc%c\t%12-15r"},
    {0x0e500110, 0x0fff0fff, "rfc%c\t%12-15r"},
    {0x0e90f110, 0x0ff8fff0, "cmf%c\t%16-18f, %0-3f"},
    {0x0eb0f110, 0x0ff8fff0, "cnf%c\t%16-18f, %0-3f"},
    {0x0ed0f110, 0x0ff8fff0, "cmfe%c\t%16-18f, %0-3f"},
    {0x0ef0f110, 0x0ff8fff0, "cnfe%c\t%16-18f, %0-3f"},
    {0x0c000100, 0x0e100f00, "stf%c%Q\t%12-14f, %A"},
    {0x0c100100, 0x0e100f00, "ldf%c%Q\t%12-14f, %A"},
    {0x0c000200, 0x0e100f00, "sfm%c\t%12-14f, %F, %A"},
    {0x0c100200, 0x0e100f00, "lfm%c\t%12-14f, %F, %A"},

    /* Generic coprocessor instructions */
    {0x0e000000, 0x0f000010, "cdp%c\t%8-11d, %20-23d, cr%12-15d, cr%16-19d, cr%0-3d, {%5-7d}"},
    {0x0e100010, 0x0f100010, "mrc%c\t%8-11d, %21-23d, %12-15r, cr%16-19d, cr%0-3d, {%5-7d}"},
    {0x0e000010, 0x0f100010, "mcr%c\t%8-11d, %21-23d, %12-15r, cr%16-19d, cr%0-3d, {%5-7d}"},
    {0x0c000000, 0x0e100000, "stc%c%22'l\t%8-11d, cr%12-15d, %A"},
    {0x0c100000, 0x0e100000, "ldc%c%22'l\t%8-11d, cr%12-15d, %A"},
    /* the rest */
    {0x00000000, 0x00000000, "undefined instruction %0-31x"},
    {0x00000000, 0x00000000, 0}
};

#define BDISP(x) ((((x) & 0xffffff) ^ 0x800000) - 0x800000)
