/*	$OpenBSD: cdefs.h,v 1.3 2005/11/24 20:46:47 deraadt Exp $	*/

/*
 * Copyright (c) 2002-2003 Opsycon AB  (www.opsycon.se / www.opsycon.com)
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */


#ifndef _MIPS_CDEFS_H_
#define	_MIPS_CDEFS_H_

#define	_C_LABEL(x)	_STRING(x)

#if defined(lint)
#define __indr_reference(sym,alias)	__lint_equal__(sym,alias)
#define __warn_references(sym,msg)
#define __weak_alias(alias,sym)		__lint_equal__(sym,alias)
#elif defined(__GNUC__) && defined(__STDC__)
#define __weak_alias(alias,sym)				\
	__asm__(".weak " __STRING(alias) " ; "		\
	    __STRING(alias) " = " __STRING(sym))
#define	__warn_references(sym,msg)			\
	__asm__(".section .gnu.warning." __STRING(sym)	\
	    " ; .ascii \"" msg "\" ; .text")
#define	__indr_references(sym,msg)	/* nothing */
#endif

#endif /* !_MIPS_CDEFS_H_ */
