/*	$OpenBSD: random_amd64.S,v 1.5 2016/02/12 21:36:33 naddy Exp $	*/

/*
 * Copyright (c) 2013 Joel Sing <jsing@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <machine/param.h>
#include <machine/asm.h>
#include <machine/psl.h>
#include <machine/specialreg.h>

/*
 * Random data is xored into the buffer in 8-byte blocks. If the buffer size
 * is not a multiple of 8, the remaining bytes may be left untouched.
 */
ENTRY(mdrandom)
	pushq   %rbx

	// See if we have CPU identification.
	pushf
	pop	%rax
	mov	%eax, %ecx
	or	$PSL_ID, %eax
	push	%rax
	popf
	pushf
	pop	%rax
	push	%rcx
	popf
	and	$PSL_ID, %eax
	jz	done

	// CPUID leaf = 1, subleaf = 0
	mov	$1, %eax
	mov	$0, %ecx
	cpuid
	mov	%edx, %eax

	movq	%rdi, %rbx
	movq	%rsi, %rdx
	xorq	%rdi, %rdi

	and	$CPUIDECX_RDRAND, %ecx	// See if we have rdrand.
	jnz	userand

	mov	%edx, %ecx
	and	$CPUID_TSC, %eax	// See if we have rdtsc.
	jnz	usetsc

	jmp	done

userand:
	shrq	$3, %rdx		// 8 bytes at a time
1:
	rdrand	%rax
	xorq	%rax, (%rbx, %rdi, 8)
	incq	%rdi
	cmpq	%rdi, %rdx
	jne	1b
	jmp	done

usetsc:
	rdtsc				// Populates edx:eax.

	/*
	 *   Cope with high=0
	 * high = (high << 1) | 1;		
	 *   Spread bits
	 * bits = low * high;
	 *   Accumulate spread bits into a byte
	 * bits = bits ^ (bits>>8) ^ (bits>>16) ^ (bits>>24);
	 * buf[i] ^= (u_char) bits;
	 */
	shlq	$1, %rdx
	orq	$1, %rdx
	mull	%edx
	movq	%rax, %rdx
	shrq	$8, %rdx
	xorq	%rdx, %rax
	shrq	$8, %rdx
	xorq	%rdx, %rax
	shrq	$8, %rdx
	xorq	%rdx, %rax

	xorb	%al, (%rbx,%rdi)

	incq	%rdi
	cmpq	%rdi, %rcx
	jne	usetsc
	jmp	done

done:
	popq	%rbx
	retq
