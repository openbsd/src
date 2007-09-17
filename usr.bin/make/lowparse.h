#ifndef LOWPARSE_H
#define LOWPARSE_H

/* $OpenPackages$ */
/* $OpenBSD: lowparse.h,v 1.5 2007/09/17 09:28:36 espie Exp $ */

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
/* low-level parsing module:
 *	select input stream to parse, and do high-speed processing of
 *	lines: skipping comments, handling continuation lines, or skipping
 *	over dead conditionals.
 *
 * Basic template:
 *
 * Parse_Fromxxx(source);
 * do {
 * 	while ((line = Parse_ReadNormalLine(&buf)) != NULL) {
 *		handle line, use Parse_Fromxxx to push includes,
 *		Parse_ReadNextConditional to get over non-conditional lines.
 *		or Parse_ReadUnparsedLine to handle special cases manually.
 * 	}
 * } while (Parse_NextFile());
 */

/* Initialization and cleanup */
#ifdef CLEANUP
extern void LowParse_Init(void);
extern void LowParse_End(void);
#else
#define LowParse_Init()
#define LowParse_End()
#endif

/* Selection of input stream */
/* Parse_FromFile(filename, filehandle);
 *	Push given filehandle on the input stack, using filename for diagnostic
 *	messages.  The module assumes ownership of the filehandle and of
 *	the filename: provide copies if necessary.  */
extern void Parse_FromFile(const char *, FILE *);
/* Parse_FromString(str, lineno);
 *	Push expanded string str on the input stack, assuming it starts at
 *	lineno in the current file.  This is used to reparse .for loops
 *	after the variable has been expanded, hence no need to respecify
 *	the filename. The module assumes ownership of the string: provide a
 *	copy if necessary.  */
extern void Parse_FromString(char *, unsigned long);

/* Error reporting, and tagging of read structures. */
/* lineno = Parse_Getlineno();
 *	Returns the current lineno. */
extern unsigned long Parse_Getlineno(void);
/* name = Parse_Getfilename();
 *	Returns the current filename.  Safe to keep without copying.  */
extern const char *Parse_Getfilename(void);

/* continue = Parse_NextFile();
 *	Advance parsing to the next file in the input stack. Returns true
 *	if there is parsing left to do.
 */
extern bool Parse_NextFile(void);


/* line = Parse_ReadNormalLine(buf);
 *	Reads next line into buffer and return its contents.  Handles line
 *	continuation, remove extra blanks, and skip trivial comments.  tabs at
 *	beginning of line are left alone, to be able to recognize target
 *	lines. */
extern char *Parse_ReadNormalLine(Buffer);

/* line = ParseReadNextConditionalLine(buf);
 *	Returns next conditional line, skipping over everything else. */
extern char *Parse_ReadNextConditionalLine(Buffer);
/* line = ParseReadUnparsedLine(buf, type);
 *	Reads line without parsing anything beyond continuations.
 *	Handle special cases such as conditional lines, or lines that
 *	need a reparse (loops). */
extern char *Parse_ReadUnparsedLine(Buffer, const char *);
/* Parse_ReportErrors();
 *	At end of parsing, report on fatal errors.
 */
extern void Parse_ReportErrors(void);
#endif
