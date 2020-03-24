/*	$OpenBSD: fgen.h,v 1.3 2020/03/24 07:00:40 otto Exp $	*/
/*	$NetBSD: fgen.h,v 1.9 2010/02/08 20:14:55 eeh Exp $	*/
/*
 * fgen.h -- stuff for the fcode tokenizer.
 *
 * Copyright (c) 1998 Eduardo Horvath.
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

/* Type of a Cell */
typedef int64_t Cell;

/* Token from the scanner. */
struct tok {
	int type;
	char *text;
};

#define TOKEN struct tok
#define YY_DECL TOKEN* yylex(void)

#define FCODE	0x000FC0DE
#define MACRO	0x0000F00D

/* Defined fcode and string. */
struct fcode {
	const char *name;
	long num;
	int type;
	struct fcode *l;
	struct fcode *r;
};

/* macro instruction as separate words */
struct macro {
	const char *name;
	const char *equiv;
	int type;
	struct macro *l;
	struct macro *r;
};

/*
 * FCode header -- assumes big-endian machine, 
 *	otherwise the bits need twiddling.
 */
struct fcode_header {
	char	header;
	char	format;
	short	checksum;
	int	length;
};

/* Tokenizer tokens */
enum toktypes { 
	TOK_OCTAL = 8,
	TOK_DECIMAL = 10,
	TOK_HEX = 16,

	TOK_NUMBER, 
	TOK_STRING_LIT, 
	TOK_C_LIT,
	TOK_PSTRING, 
	TOK_TOKENIZE,
	TOK_COMMENT, 
	TOK_COLON, 
	TOK_SEMICOLON, 
	TOK_TOSTRING,
	
	/* These are special */
	TOK_ABORT_S,
	TOK_AGAIN,
	TOK_ALIAS,
	TOK_GETTOKEN,
	TOK_ASCII,
	TOK_BEGIN,
	TOK_BUFFER,
	TOK_CASE,
	TOK_CONSTANT,
	TOK_CONTROL,
	TOK_CREATE,
	TOK_DEFER,
	TOK_DO,
	TOK_ELSE,
	TOK_END0,
	TOK_ENDCASE,
	TOK_ENDOF,
	TOK_EXTERNAL,
	TOK_FCODE_VERSION2,
	TOK_FCODE_END,
	TOK_FIELD,
	TOK_HEADERLESS,
	TOK_HEADERS,
	TOK_IF,
	TOK_LEAVE,
	TOK_LOOP,
	TOK_OF,
	TOK_OFFSET16,
	TOK_REPEAT,
	TOK_STARTX,
	TOK_THEN,
	TOK_TO,
	TOK_UNTIL,
	TOK_VALUE,
	TOK_VARIABLE,
	TOK_VERSION1,
	TOK_WHILE,

	/* Tokenizer directives */
	TOK_BEGTOK,
	TOK_EMIT_BYTE,
	TOK_ENDTOK,
	TOK_FLOAD,

	TOK_OTHER
};
