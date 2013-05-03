/*      $OpenBSD: citrus_ctype_local.h,v 1.4 2013/05/03 13:53:49 stsp Exp $       */
/*      $NetBSD: citrus_ctype_local.h,v 1.2 2003/03/05 20:18:15 tshiozak Exp $  */

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
 *
 */

#ifndef _CITRUS_CTYPE_LOCAL_H_
#define _CITRUS_CTYPE_LOCAL_H_

#define _CITRUS_CTYPE_DECLS(_e_)					\
size_t	_citrus_##_e_##_ctype_mbrtowc(wchar_t * __restrict,		\
				      const char * __restrict, size_t,	\
				      void * __restrict);		\
int	_citrus_##_e_##_ctype_mbsinit(const void * __restrict);		\
size_t	_citrus_##_e_##_ctype_mbsnrtowcs(wchar_t * __restrict,		\
					 const char ** __restrict,	\
					 size_t, size_t,		\
					 void * __restrict);		\
size_t	_citrus_##_e_##_ctype_wcrtomb(char * __restrict, wchar_t,	\
				      void * __restrict);		\
size_t	_citrus_##_e_##_ctype_wcsnrtombs(char * __restrict,		\
					 const wchar_t ** __restrict,	\
					 size_t, size_t,		\
					 void * __restrict);		\

#define _CITRUS_CTYPE_DEF_OPS(_e_)					\
struct _citrus_ctype_ops_rec _citrus_##_e_##_ctype_ops = {		\
	/* co_mbrtowc */	&_citrus_##_e_##_ctype_mbrtowc,		\
	/* co_mbsinit */	&_citrus_##_e_##_ctype_mbsinit,		\
	/* co_mbsnrtowcs */	&_citrus_##_e_##_ctype_mbsnrtowcs,	\
	/* co_wcrtomb */	&_citrus_##_e_##_ctype_wcrtomb,		\
	/* co_wcsnrtombs */	&_citrus_##_e_##_ctype_wcsnrtombs,	\
}

typedef size_t	(*_citrus_ctype_mbrtowc_t)
	(wchar_t * __restrict, const char * __restrict,
	size_t, void * __restrict);
typedef int	(*_citrus_ctype_mbsinit_t) (const void * __restrict);
typedef size_t	(*_citrus_ctype_mbsnrtowcs_t)
	(wchar_t * __restrict, const char ** __restrict,
	 size_t, size_t, void * __restrict);
typedef size_t	(*_citrus_ctype_wcrtomb_t)
	(char * __restrict, wchar_t, void * __restrict);
typedef size_t	(*_citrus_ctype_wcsnrtombs_t)
	(char * __restrict, const wchar_t ** __restrict,
	 size_t, size_t, void * __restrict);

struct _citrus_ctype_ops_rec {
	_citrus_ctype_mbrtowc_t		co_mbrtowc;
	_citrus_ctype_mbsinit_t		co_mbsinit;
	_citrus_ctype_mbsnrtowcs_t	co_mbsnrtowcs;
	_citrus_ctype_wcrtomb_t		co_wcrtomb;
	_citrus_ctype_wcsnrtombs_t	co_wcsnrtombs;
};

#define _CITRUS_DEFAULT_CTYPE_NAME	"NONE"

struct _citrus_ctype_rec {
	struct _citrus_ctype_ops_rec	*cc_ops;
	size_t				cc_mb_cur_max;
};

#endif
