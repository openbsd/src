/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $arla: sym.h,v 1.10 2002/04/15 14:53:19 lha Exp $ */

#ifndef _SYM_
#define _SYM_

#include <bool.h>
#include <list.h>

struct Type;

typedef struct Type Type;

typedef enum
{
     YDR_TUNDEFINED, YDR_TSTRUCT, YDR_TENUM, YDR_TCONST, YDR_TENUMVAL,
     YDR_TTYPEDEF, YDR_TPROC
} SymbolType;

enum { TSPLIT = 1, TSIMPLE = 2, TMULTI = 4};

typedef struct {
     SymbolType type;
     char *name;
     union {
 	  List *list;
	  int val;
	  Type *type;
	  struct {
	      unsigned id;
	      List *arguments;
	      unsigned flags;
	      char *package;
	  } proc;
     } u;
     List *attrs;
} Symbol;

void initsym (void);
Symbol* addsym (char *);
Symbol* findsym (char *);
void symiterate (Bool (*func)(void *, void *), void *arg);

#endif /* _SYM_ */
