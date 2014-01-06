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
 * Buffer size must be multiple of 4
 */
ENTRY(mdrandom)
	pushal

	// See if we have CPU identification.
	pushfl
	popl	%eax
	movl	%eax, %ecx
	orl	$PSL_ID, %eax
	pushl	%eax
	popfl
	pushfl
	popl	%eax
	pushl	%ecx
	popfl
	andl	$PSL_ID, %eax
	jz	done

	// CPUID leaf = 0, subleaf = 0
	movl	$0, %eax
	movl	$0, %ecx
	cpuid
	cmpl	$0x756e6547, %ebx	// "Genu"
	jne	done
	cmpl	$0x6c65746e, %ecx	// "ineI"
	jne	done
	cmpl	$0x49656e69, %edx	// "ntel"
	jne	done

	// CPUID leaf = 1, subleaf = 0
	movl	$1, %eax
	movl	$0, %ecx
	cpuid

	movl	36(%esp), %ebx
	movl	40(%esp), %edx
	shrl	$2, %edx
	xorl	%edi, %edi

	andl	$CPUIDECX_RDRAND, %ecx	// See if we have rdrand.
	jnz	userand

	movl	%edx, %ecx
	jmp	usetsc

userand:
	rdrand	%eax
	xorl	%eax, (%ebx,%edi,4)
	incl	%edi
	cmpl	%edi, %edx
	jne	userand
	jmp	done

usetsc:
	rdtsc				// Populates edx:eax.
	mull	%edx
	xorl	%eax, (%ebx,%edi,4)
	incl	%edi
	cmpl	%edi, %ecx
	jne	usetsc
	jmp	done

done:
	popal
	ret
