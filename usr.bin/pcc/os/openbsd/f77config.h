/*	$Id: f77config.h,v 1.1.1.1 2007/09/15 18:12:37 otto Exp $	*/

/*
 * Copyright (c) 2004 Anders Magnusson (ragge@ludd.luth.se).
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 * Various settings that controls how the f77 compiler works.
 */

#define PASS1NAME	"/usr/lib/f77pass1"
#define PASS2NAME	"/lib/f1"
#define PASS2OPT	"/lib/c2"
#define NOFLPASS2	"/lib/fc1"

#define ASMNAME		"/usr/bin/as"
#define LDNAME		"/usr/bin/ld"
#define FOOTNAME	"/usr/lib/crt0.o"
#define PROFFOOT	"/usr/lib/mcrt0.o"
#define NOFLFOOT	"/usr/lib/fcrt0.o"
#define NOFLPROFFOOT	"/usr/lib/fmcrt0.o"
#define LIBLIST		{ "-lF77", "-lI77", "-lm", "-lc", "-l", NULL };

