#ifndef SUFF_H
#define SUFF_H
/*	$OpenPackages$ */
/*	$OpenBSD: suff.h,v 1.3 2007/09/17 12:42:09 espie Exp $ */

/*
 * Copyright (c) 2001 Marc Espie.
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
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

extern void Suff_ClearSuffixes(void);
extern GNode *Suff_ParseAsTransform(const char *, const char *);
struct Suff_;
extern void Suff_AddSuffixi(const char *, const char *);
extern void Suff_AddIncludei(const char *, const char *);
extern void Suff_AddLibi(const char *, const char *);
extern void Suff_FindDeps(GNode *);
extern void Suff_SetNulli(const char *, const char *);
extern void Suff_Init(void);
extern void process_suffixes_after_makefile_is_read(void);
extern Lst find_suffix_path(GNode *);
#ifdef CLEANUP
extern void Suff_End(void);
#else
#define Suff_End()
#endif
extern void Suff_PrintAll(void);

#endif
