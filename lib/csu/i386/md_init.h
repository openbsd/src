/* $OpenBSD: md_init.h,v 1.1 2004/01/08 14:59:15 drahn Exp $ */

/*-
 * Copyright (c) 2001 Ross Harvey
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
 *      This product includes software developed by the NetBSD
 *      Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define MD_SECT_CALL_FUNC(section, func) \
	__asm (".section "#section", \"ax\"\n"	\
	"	call " #func "\n"		\
	"	.previous")

/*
 * Align is after because we want the function to start at the first
 * address of the section, but overall we want the section to be
 * aligned by the align amount.
 */
#define MD_SECTION_PROLOGUE(sect, entry_pt)	\
	__asm (					\
	".section "#sect",\"ax\",@progbits	\n" \
	"	.globl " #entry_pt "		\n" \
	"	.type " #entry_pt ",@function	\n" \
	#entry_pt":				\n" \
	"	.align 16			\n" \
	"	/* fall thru */			\n" \
	"	.previous")


#define MD_SECTION_EPILOGUE(sect)		\
	__asm (					\
	".section "#sect",\"ax\",@progbits	\n" \
	"	ret				\n" \
	"	.previous")
