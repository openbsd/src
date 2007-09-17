#ifndef PARSE_H
#define PARSE_H
/*	$OpenPackages$ */
/*	$OpenBSD: parse.h,v 1.3 2007/09/17 11:11:30 espie Exp $ */
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

/* parse
 *	Functions to parse Makefiles and fragments.
 */

extern void Parse_Init(void);
#ifdef CLEANUP
extern void Parse_End(void);
#else
#define Parse_End()
#endif

extern Lst	systemIncludePath;
extern Lst	userIncludePath;

/* Parse_File(filename, filehandle);
 *	Parses stream filehandle, use filename when reporting error
 *	messages.  Builds a graph of GNode and Suffixes. This modules
 *	acquires ownership of the filename and filehandle, and will
 *	free/close them when it sees fit. */
extern void Parse_File(const char *, FILE *);

/* Parse_AddIncludeDir(dir);
 *	Adds directory dir to user include"" path (copy the string). */
extern void Parse_AddIncludeDir(const char *);

/* Parse_MainName(lst);
 *	Adds set of main targets to create to list lst. Errors out if no
 *	target exists. */
extern void Parse_MainName(Lst);

#endif
