#ifndef STR_H
#define STR_H
/* $OpenBSD: str.h,v 1.2 2010/07/19 19:46:44 espie Exp $ */
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

/* pos = Str_rchri(str, end, c);
 *	strrchr on intervals.  */
extern char *Str_rchri(const char *, const char *, int);

/* copy = Str_concati(s1, e1, s2, e2, sep);
 *	Concatenate strings s1/e1 and s2/e2, wedging separator sep if != 0.  */
extern char *Str_concati(const char *, const char *, const char *, const char *, int);
#define Str_concat(s1, s2, sep) Str_concati(s1, strchr(s1, '\0'), s2, strchr(s2, '\0'), sep)

/* copy = Str_dupi(str, end);
 *	strdup on intervals.  */
extern char *Str_dupi(const char *, const char *);

/* copy = escape_dupi(str, end, set);
 *	copy string str/end. All escape sequences such as \c with c in set
 *	are handled as well.  */
extern char *escape_dupi(const char *, const char *, const char *);


extern char **brk_string(const char *, int *, char **);


/* Iterate through a string word by word,
 * without copying anything.
 * More light-weight than brk_string, handles \ ' " as well.
 *
 * position = s;
 * while ((begin = iterate_words(&position)) != NULL) {
 *   do_something_with_word_interval(begin, position);
 * }
 */
extern const char *iterate_words(const char **);

/* match = Str_Matchi(str, estr, pat, end);
 *	Checks if string str/estr matches pattern pat/end */
extern bool Str_Matchi(const char *, const char *, const char *, const char *);
#define Str_Match(string, pattern) \
	Str_Matchi(string, strchr(string, '\0'), pattern, strchr(pattern, '\0'))

extern const char *Str_SYSVMatch(const char *, const char *, size_t *);
extern void Str_SYSVSubst(Buffer, const char *, const char *, size_t);
#endif
