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
 * Random data is xored into the buffer in 4 byte blocks. If the buffer size
 * is not a multiple of 4, the remaining bytes will be left untouched.
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

	// CPUID leaf = 0, subleaf = 0
	mov	$0, %eax
	mov	$0, %ecx
	cpuid
	cmp	$0x756e6547, %ebx	// "Genu"
	jne	done
	cmp	$0x6c65746e, %ecx	// "ineI"
	jne	done
	cmp	$0x49656e69, %edx	// "ntel"
	jne	done

	// CPUID leaf = 1, subleaf = 0
	mov	$1, %eax
	mov	$0, %ecx
	cpuid
	mov	%edx, %eax

	movq	%rdi, %rbx
	movq	%rsi, %rdx
	shrq	$3, %rdx
	xorq	%rdi, %rdi

	and	$CPUIDECX_RDRAND, %ecx	// See if we have rdrand.
	jnz	userand

	mov	%edx, %ecx
	and	$CPUID_TSC, %eax	// See if we have rdtsc.
	jnz	usetsc

	jmp	done

userand:
	rdrand	%rax
	xorq	%rax, (%rbx, %rdi, 8)
	incq	%rdi
	cmpq	%rdi, %rdx
	jne	userand
	jmp	done

usetsc:
	rdtsc				// Populates edx:eax.
	mulq	%rdx
	xorq	%rax, (%rbx, %rdi, 8)
	incq	%rdi
	cmpq	%rdi, %rcx
	jne	usetsc
	jmp	done

done:
	popq	%rbx
	retq
