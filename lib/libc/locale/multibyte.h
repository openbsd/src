/*	$OpenBSD: multibyte.h,v 1.1 2010/07/27 16:59:04 stsp Exp $ */
/*	$NetBSD: multibyte.h,v 1.5 2009/01/11 02:46:28 christos Exp $	*/

/*-
 * Copyright (c)2002 Citrus Project,
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _MULTIBYTE_H_
#define _MULTIBYTE_H_

typedef struct _RuneStatePriv {
	_RuneLocale	*__runelocale;
	char		__private __attribute__((__aligned__));
} _RuneStatePriv;

typedef union _RuneState {
	mbstate_t		__pad;
	struct _RuneStatePriv	__priv;
#define rs_runelocale		__priv.__runelocale
#define rs_private		__priv.__private
} _RuneState;
#define _RUNE_STATE_PRIVSIZE	(sizeof(mbstate_t)-offsetof(_RuneStatePriv, __private))

#define _ps_to_runestate(ps)	((_RuneState *)(void *)(ps))
#define _ps_to_runelocale(ps)	(_ps_to_runestate(ps)->rs_runelocale)
#define _ps_to_private(ps)	((void *)&_ps_to_runestate(ps)->rs_private)
	
#endif /*_MULTIBYTE_H_*/
