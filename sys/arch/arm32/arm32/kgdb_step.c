/*	$NetBSD: kgdb_step.c,v 1.2 1996/03/27 22:42:20 mark Exp $	*/

/*
 * Copyright (C) 1994 Wolfgang Solfrank.
 * Copyright (C) 1994 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>

#include <machine/cpu.h>
#include <machine/psl.h>
#include <machine/kgdb.h>

/*
 * Faults during instruction fetch? XXX
 */
int
fetchinst(pc)
	void *pc;
{
	int inst, byte, n;
	
	inst = 0;
	pc += sizeof(int);
	for (n = sizeof(int); --n >= 0;) {
		inst <<= 8;
		byte = kgdbfbyte(--pc);
		if (byte < 0)
			return 0xe7ffffff; /* special hack! */
		inst |= byte;
	}
	return inst;
}

static __inline void
execute(inst, args, regs)
	int inst;
	int *args;
	int *regs;
{
	int *sp;
	
	/*
	 * For now, no user level emulation
	 */
	if (PSR_USER(regs[PSR]) || !PSR_32(regs[PSR]))
		panic("execute");
	sp = kgdb_find_stack();
	regs[PSR] = Execute(inst, regs[PSR], args, sp);
	kgdb_free_stack(sp);
}

static __inline int
condition(inst, args, regs)
	int inst;
	int *args;
	int *regs;
{
	args[0] = 0;
	/* mov{cond} r0, #1 */
	execute((inst&0xf0000000)|0x03a00001, args, regs);
	return args[0];
}

static __inline int
immediate(inst)
	int inst;
{
	int imm = inst&0xff;
	int rot = (inst >> 8)&0xf;
	
	rot *= 2;
	return (imm >> rot)|(imm << (32 - rot));
}

static __inline int
getreg(reg, ahead, regs)
	int reg;
	int ahead;
	int *regs;
{
	if (reg == PC)
		return regs[PC] + (ahead ? 12 : 8);
	return regs[reg];
}

static __inline void
setreg(reg, val, regs)
	int reg;
	int val;
	int *regs;
{
	if (reg == PC)
		val &= ~3;
	regs[reg] = val;
}

int
singlestep(regs)
	int *regs;
{
	int inst;
	int args[5];
	int dst, idx;
	int val;
	
	inst = fetchinst(regs[PC]);
	switch (inst&0x0c000000) {
	case 0x00000000:
		if ((inst&0x0fb00ff0) == 0x01000090) {
			/* swp */
			dst = (inst >> 12)&0xf;
			if (dst == PC)
				return -1;
			idx = inst&0xf;
			if (idx == PC)
				return -1;
			args[0] = getreg(idx, 0, regs);
			args[1] = getreg(dst, 0, regs);
			idx = (inst >> 16)&0xf;
			if (idx == PC)
				return -1;
			args[2] = getreg(idx, 0, regs);
			execute((inst&0xf0400000)|0x01021090, args, regs);
			setreg(dst, args[1], regs);
			return 4;
		}
		if ((inst&0x0fbf0fff) == 0x01000090) {
			/* mrs */
			if ((regs[PSR]&0xf) == 0
			    && (inst&0x00400000))
				/* mrs xx, spsr in user mode */
				return 4; /* ??? */
			dst = (inst >> 12)&0xf;
			if (dst == PC)
				return -1;
			if (condition(inst, args, regs))
				setreg(dst, regs[(inst&0x00400000) ? SPSR : PSR], regs);
			return 4;
		}
		if ((inst&0x0fbffff0) == 0x0129f000) {
			/* msr */
			if (condition(inst, args, regs)) {
				idx = inst&0xf;
				if (idx == PC)
					return -1;
				val = getreg(idx, 0, regs);
				if ((regs[PSR]&0xf) == 0) {
					if (inst&0x00400000)
						/* msr spsr, xx in user mode */
						return 4; /* ??? */
					val &= 0xf0000000;
					val |= regs[PSR]&0x0fffffff;
				}
				regs[(inst&0x00400000) ? SPSR : PSR] = val;
			}
			return 4;
		}
		if ((inst&0x0dbff000) == 0x0128f000) {
			/* msrf */
			if (condition(inst, args, regs)) {
				if ((regs[PSR]&0xf) == 0
				    && (inst&0x00400000))
					/* msr spsr_flg, xx in user mode */
					return 4; /* ??? */
				if (inst&0x02000000)
					val = immediate(inst);
				else if (inst&0x00000ff0)
					return -1;
				else {
					idx = inst&0xf;
					if (idx == PC)
						return -1;
					val = getreg(idx, 0, regs);
				}
				val &= 0xf0000000;
				val |= regs[PSR]&0x0fffffff;
				regs[(inst&0x00400000) ? SPSR : PSR] = val;
			}
			return 4;
		}
		if ((inst&0x0fc000f0) == 0x00000090) {
			/* mul/mla */
			dst = (inst >> 16)&0xf;
			if (dst == PC)
				return -1;
			idx = inst&0xf;
			if (idx == dst || idx == PC)
				return -1;
			args[1] = getreg(idx, 0, regs);
			idx = (inst >> 8)&0xf;
			if (idx == PC)
				return -1;
			args[2] = getreg(idx, 0, regs);
			idx = (inst >> 12)&0xf;
			if (idx == PC)
				return -1;
			if (idx && !(inst&0x00200000))
				/* mul with rn != 0 */
				return -1;
			args[0] = getreg(idx, 0, regs);
			args[3] = getreg(dst, 0, regs);
			execute((inst&0xfff00000)|0x30291, args, regs);
			setreg(dst, args[3], regs);
			return 4;
		}
		{
			/* data processing */
			if (condition(inst, args, regs)) {
				dst = (inst >> 12)&0xf;
				if (inst&0x00100000) {
					/* S-Bit set */
					if (dst == PC)
						/* S-Bit set and destination is R15 */
						return -1; /* not yet */
				} else
					/* S-Bit not set */
					switch ((inst >> 21)&0xf) {
					case 0x8: /* TST */
					case 0x9: /* TEQ */
					case 0xa: /* CMP */
					case 0xb: /* CMN */
						return -1;
					}
				val = ((inst&0x02000010) == 0x00000010);
				args[0] = getreg((inst >> 16)&0xf, val, regs);
				if (!(inst&0x02000000)) {
					args[2] = getreg(inst&0xf, val, regs);
					if (inst&0x00000010) {
						if (inst&0x00000080)
							return -1;
						args[3] = getreg((inst >> 8)&0xf, val, regs);
						inst = (inst&0xfff000f0)|0x00001302;
					} else
						inst = (inst&0xfff00ff0)|0x00001002;
				} else
					inst = (inst&0xfff00fff)|0x00001000;
				execute(inst, args, regs);
				switch ((inst >> 21)&0xf) {
				case 0x8: /* TST */
				case 0x9: /* TEQ */
				case 0xa: /* CMP */
				case 0xb: /* CMN */
					break;
				default:
					setreg(dst, args[1], regs);
					break;
				}
				return dst == PC ? 0 : 4;
			}
			return 4;
		}
		break;
	case 0x04000000:
		if ((inst&0x0e000010) == 0x06000010)
			/* undefined */
			return -1;
		{
			/* ldr/str */
			if (condition(inst, args, regs)) {
				dst = (inst >> 12)&0xf;
				if (inst&0x00100000)
					args[1] = regs[dst];
				else
					args[1] = getreg(dst, 1, regs);
				val = (inst >> 16)&0xf;
				if ((inst&0x00200000) && val == PC)
					/* write back with pc as base */
					return -1;
				args[2] = getreg(val, 0, regs);
				if (inst&0x02000000) {
					if (inst&0x00000010)
						/* shift amount in register */
						return -1;
					idx = inst&0xf;
					if (idx == PC)
						/* offset in PC */
						return -1;
					args[0] = getreg(idx, 0, regs);
					inst = (inst&0xfff00ff0)|0x21000;
				} else
					inst = (inst&0xfff00fff)|0x21000;
				execute(inst, args, regs);
				if (inst&0x00200000)
					regs[val] = args[2];
				if (inst&0x00100000)
					setreg(dst, args[1], regs);
				return dst == PC ? 0 : 4;
			}
			return 4;
		}
		break;
	case 0x08000000:
		switch (inst&0x0e000000) {
		case 0x08000000:
			/* ldm/stm */
			if (condition(inst, args, regs)) {
				int cnt;
				int val, is1;
				int final;
				
				if (inst&0x00400000)
					/* S-bit not yet supported */
					return -1;
				dst = (inst >> 16)&0xf;
				if (dst == PC)
					return -1;
				args[0] = final = val = getreg(dst, 0, regs);
				cnt = 0;
				is1 = 0;
				for (idx = 0; idx < 16; idx++)
					if ((inst&(1 << idx))
					    && (cnt += 4) <= 4 /* count the registers */
					    && idx == dst)
						/* indicate unmodified store */
						is1 = 1;
				if ((inst&0x00300000) == 0x00200000
				    && (inst&(1 << dst))
				    && !is1) {
					/*
					 * The destination is in the list of a stm
					 * with write-back and is not the first
					 * register
					 */
					if (inst&0x00800000)
						val -= cnt;
					else
						val += cnt;
				}
				if (!(inst&0x00800000)) {
					/* lowest address involved */
					args[0] -= cnt;
					if (!(inst&0x01000000))
						/* post-decrement */
						args[0] += 4;
					if (inst&0x00200000)
						final -= cnt;
				} else {
					if (inst&0x01000000)
						/* pre-increment */
						args[0] += 4;
					if (inst&0x00200000)
						final += cnt;
				}
				for (idx = 0; idx < 16; idx++)
					if (inst&(1 << idx)) {
						args[1] = dst == idx
							  ? val
							  : getreg(idx, 1, regs);
						execute((inst&0xfe100000)|0x00a00002, args, regs);
						if (inst&0x00100000)
							regs[idx] = args[1];
					}
				switch (inst&0x00300000) {
				case 0x00300000: /* ldm! */
					if (inst&(1 << dst))
						break;
				case 0x00200000: /* stm! */
					regs[dst] = final;
					break;
				}
				return (inst&(1 << PC)) && (inst&0x00100000) ? 0 : 4;
			}
			return 4;
		case 0x0a000000:
			/* branch */
			if (condition(inst, args, regs)) {
				if (inst&0x01000000)
					regs[LR] = regs[PC] + 4;
				if (inst&0x00800000)
					inst |= 0xff000000;
				else
					inst &= ~0xff000000;
				regs[PC] += 8 + (inst << 2);
				return 0;
			}
			return 4;
		}
		break;
	case 0x0c000000:
		switch (inst&0x0f000000) {
		case 0x0c000000:
		case 0x0d000000:
			/* ldc/stc */
			return -1;
		case 0x0f000000:
			/* swi */
			return -1;
		case 0x0e000000:
			/* cdp/mrc/mcr */
			if ((regs[PSR]&0xf) == 0)
				/* user mode */
				return -1;
			if ((inst&0x00e00fff) != 0x00000f10)
				/* cdp, different cp# or unknown operation */
				return -1;
			dst = (inst >> 12)&0xf;
			if (dst == PC)
				args[0] = regs[PSR];
			else
				args[0] = getreg(dst, 1, regs);
			execute(inst&0xffff0fff, args, regs);
			if (!(inst&0x00100000))
				if (dst == PC)
					regs[PSR] = (regs[PSR]&0x0fffffff)
							|(args[0]&0xf0000000);
				else
					regs[dst] = args[0];
			return 4;
		}
		break;
	}
	return -1;
}
