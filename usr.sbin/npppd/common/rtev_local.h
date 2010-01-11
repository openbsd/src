/*-
 * Copyright (c) 2009 Internet Initiative Japan Inc.
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
#ifndef RTEV_LOCAL_H
#define RTEV_LOCAL_H

#ifdef	RTEV_DEBUG
#include "debugutil.h"
#define	RTEV_DBG(x)	log_printf x
#define	RTEV_ASSERT(cond)					\
	if (!(cond)) {						\
	    fprintf(stderr,					\
		"\nASSERT(" #cond ") failed on %s() at %s:%d.\n"\
		, __func__, __FILE__, __LINE__);		\
	    abort(); 						\
	}
#else
#define	RTEV_ASSERT(cond)			
#define	RTEV_DBG(x)
#define	log_printf	syslog
#endif

typedef struct _rtev_impl rtev_impl;
struct _rtev_impl {
	void *impl;
	void (*impl_fini)(rtev_impl *);
	void (*impl_on_write)(rtev_impl *);
	int  (*base_on_rtevent)(rtev_impl *);
	int  (*base_on_write)(rtev_impl *, int, int);
	int  base_flags;
};

int rtev_base_init (rtev_impl *, int);

#endif
