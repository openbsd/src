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
#ifndef PROPERTIES_H
#define PROPERTIES_H 1

struct properties;

#ifndef	__P
#if defined(__STDC__) || defined(_MSC_VER)
#define	__P(x)	x
#else
#define	__P(x)	()
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct properties  *properties_create __P((int));
const char         *properties_get __P((struct properties *, const char *));
void               properties_remove __P((struct properties *, const char *));
void               properties_remove_all __P((struct properties *));
const char         *properties_put __P((struct properties *, const char *, const char *));
void		   properties_put_all __P((struct properties *, struct properties *));
void               properties_destroy __P((struct properties *));
const char         *properties_first_key __P((struct properties *));
const char         *properties_next_key __P((struct properties *));
int                properties_save __P((struct properties *, FILE *));
int                properties_load __P((struct properties *, FILE *));

#ifdef __cplusplus
}
#endif

#endif
