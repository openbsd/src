/* $OpenBSD: md_init.h,v 1.1 2005/04/01 10:56:43 mickey Exp $ */

/*
 * Copyright (c) 2003 Dale Rahn. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */  


#define MD_SECT_CALL_FUNC(section, func)			\
	__asm (".section "#section",\"ax\",@progbits	\n"	\
	"	bl	" #func ",%r2			\n"	\
	"	std	%r19,-80(%r30)			\n"	\
	"	ldd	-80(%r30),%r19			\n"	\
	"	.previous")

#define MD_SECTION_PROLOGUE(sect, entry_pt)			\
	__asm (						   	\
	"	.section "#sect",\"ax\",@progbits	\n"	\
	"	.EXPORT "#entry_pt",ENTRY,PRIV_LEV=3,ARGW0=NO,ARGW1=NO,ARGW2=NO,ARGW3=NO,RTNVAL=NO					\n"	\
	"	.align 4				\n"	\
	#entry_pt"					\n"	\
	"	std %r2, -20(%r30)			\n"	\
	"	ldo 64(%r30),%r30			\n"	\
	"	/* fall thru */				\n"	\
	"	.previous")


#define MD_SECTION_EPILOGUE(sect)				\
	__asm (							\
	"	.section "#sect",\"ax\",@progbits	\n"	\
	"	ldd -84(%r30),%r2			\n"	\
	"	bv %r0(%r2)				\n"	\
	"	ldo -64(%r30),%r30			\n"	\
	"	.previous")
