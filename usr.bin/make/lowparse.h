/* $OpenBSD: lowparse.h,v 1.1 2000/06/23 16:39:45 espie Exp $ */

/*
 * Copyright (c) 1999 Marc Espie.
 *
 * Extensive code changes for the OpenBSD project.
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
#ifndef LOWPARSE_H
#define LOWPARSE_H
extern void Parse_FromFile __P((char *, FILE *));
extern Boolean Parse_NextFile __P((void));
#ifdef CLEANUP
extern void LowParse_Init __P((void));
extern void LowParse_End __P((void));
#endif
extern void Finish_Errors __P((void));
extern void ParseUnreadc __P((char));

/* Definitions for handling #include specifications */
typedef struct IFile_ {
    char           	*fname;	/* name of file */
    unsigned long      	lineno;	/* line number */
    FILE 		*F;	/* open stream */
    char 		*str;	/* read from char area */	
    char 		*ptr;	/* where we are */
    char 		*end;	/* don't overdo it */
} IFile;

IFile	*current;

int newline __P((void));
#define ParseReadc()	current->ptr < current->end ? *current->ptr++ : newline()

#endif
