/* $OpenBSD: sysdep.h,v 1.25 2005/05/04 10:05:01 hshoexer Exp $	 */
/* $EOM: sysdep.h,v 1.17 2000/12/04 04:46:35 angelos Exp $	 */

/*
 * Copyright (c) 1998, 1999 Niklas Hallqvist.  All rights reserved.
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

/*
 * This code was written under funding by Ericsson Radio Systems.
 */

#ifndef _SYSDEP_H_
#define _SYSDEP_H_

#include <sys/types.h>
#if defined (USE_BOEHM_GC)
#include <stdlib.h>
#include <string.h>
#endif

extern int      sysdep_cleartext(int, int);

#if defined (USE_BOEHM_GC)
/*
 * Use Boehm's garbage collector as a means to find leaks.
 * XXX The defines below are GCC-specific.  I think it is OK to require
 * XXX GCC if you are debugging isakmpd in this way.
 */
void	*GC_debug_malloc(size_t, char *, int);
void	*GC_debug_realloc(void *, size_t, char *, int);
void	 GC_debug_free(void *);
char	*gc_strdup(const char *);

#define malloc(x)	GC_debug_malloc ((x), __FILE__, __LINE__)
#define realloc(x,y)	GC_debug_realloc ((x), (y), __FILE__, __LINE__)
#define free(x)		GC_debug_free (x)
#define calloc(x,y)	malloc((x) * (y))
#define strdup(x)	gc_strdup((x))

#endif /* WITH_BOEHM_GC */

#endif				/* _SYSDEP_H_ */
