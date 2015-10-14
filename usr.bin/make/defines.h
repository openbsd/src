#ifndef DEFINES_H
#define DEFINES_H

/*	$OpenBSD: defines.h,v 1.15 2015/10/14 13:50:22 espie Exp $ */

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

# include <stdbool.h>

/* define common types in an opaque way */
struct GNode_;
typedef struct GNode_ GNode;

struct Location_;
typedef struct Location_ Location;

struct List_;
typedef struct List_ *Lst;

struct SymTable_;
typedef struct SymTable_ SymTable;

struct Buffer_;
typedef struct Buffer_ *Buffer;

struct Name;

struct ListNode_;
typedef struct ListNode_ *LstNode;

struct Job_;
typedef struct Job_ Job;

struct Suff_;
typedef struct Suff_ Suff;

/* some useful defines for gcc */

#ifdef __GNUC__
# define UNUSED	__attribute__((__unused__))
# define HAS_INLINES
# define INLINE  __inline__
#else
# define UNUSED
#endif

#ifdef HAS_INLINES
# ifndef INLINE
#  define INLINE	inline
# endif
#endif

/*
 * debug control:
 *	There is one bit per module.  It is up to the module what debug
 *	information to print.
 */
extern int debug;
#define DEBUG_ARCH	0x0001
#define DEBUG_COND	0x0002
#define DEBUG_DIR	0x0004
#define DEBUG_GRAPH1	0x0008
#define DEBUG_GRAPH2	0x0010
#define DEBUG_JOB	0x0020
#define DEBUG_MAKE	0x0040
#define DEBUG_SUFF	0x0080
#define DEBUG_TARG	0x0100
#define DEBUG_VAR	0x0200
#define DEBUG_FOR	0x0400
#define DEBUG_LOUD	0x0800
#define DEBUG_PARALLEL	0x1000
#define DEBUG_NAME_MATCHING	0x2000
#define DEBUG_QUICKDEATH	0x4000
#define DEBUG_EXPENSIVE		0x8000
#define DEBUG_KILL		0x10000
#define DEBUG_HELDJOBS		0x20000
#define DEBUG_DOUBLE		0x40000
#define DEBUG_TARGGROUP		0x80000

#define CONCAT(a,b)	a##b

#define DEBUG(module)	(debug & CONCAT(DEBUG_,module))

#define ISLOWER(c) (islower((unsigned char)(c)))
#define ISUPPER(c) (isupper((unsigned char)(c)))
#define ISDIGIT(c) (isdigit((unsigned char)(c)))
#define ISXDIGIT(c) (isxdigit((unsigned char)(c)))
#define ISSPACE(c) (isspace((unsigned char)(c)))
#define TOUPPER(c) (toupper((unsigned char)(c)))
#define TOLOWER(c) (tolower((unsigned char)(c)))

#endif
