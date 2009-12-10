%{
/*	$OpenBSD: fgen.l,v 1.9 2009/12/10 17:31:49 deraadt Exp $	*/
/*	$NetBSD: fgen.l,v 1.12 2001/06/13 10:46:05 wiz Exp $	*/
/* FLEX input for FORTH input file scanner */
/*  
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Eduardo Horvath.
 * 4. The name of the author may not be used to endorse or promote products
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
	Specifications are as follows:

	The function "yylex()" always returns a pointer to a structure:

	    struct tok {
		int type;
		char *text;
	    }
	    #define TOKEN struct tok
*/
%}

%option yylineno

decimal	[0-9]
hex	[0-9A-Fa-f]
octal	[0-7]
white	[ \t\n\r\f]
tail	{white}

%{
#include <sys/types.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "fgen.h"
TOKEN token;

/*
 * Global variables that control the parse state.
 */

struct fcode *dictionary = NULL;
struct macro *aliases = NULL;
int outf = 1; /* stdout */
int state = 0;
int nextfcode = 0x800; 
int base = TOK_HEX;
long outpos;
char *outbuf = NULL;
char *outfile, *infile;
#define BUFCLICK	(1024*1024)
size_t outbufsiz = 0;
char *myname = NULL;
int offsetsize = 8;
int defining = 0;
int tokenizer = 0;

#define PSTKSIZ		1024
Cell parse_stack[PSTKSIZ];
int parse_stack_ptr = 0;

void	token_err(int, char *, char *, char *, ...)
	__attribute__((__format__(__printf__, 4, 5)));
YY_DECL;

int debug = 0;
#define ASSERT if (debug) assert
#define STATE(y, x)	do { if (debug) printf( "%ld State %s: token `%s'\n", outpos, x, y); } while (0)

#define YY_NO_UNPUT
%}

%%

0			{ token.type = TOK_OTHER; token.text = yytext;
				return &token; }

1			{ token.type = TOK_OTHER; token.text = yytext;
				return &token; }

2			{ token.type = TOK_OTHER; token.text = yytext;
				return &token; }

3			{ token.type = TOK_OTHER; token.text = yytext;
				return &token; }

-1		{ token.type = TOK_OTHER; token.text = yytext;
				return &token; }

{white}*		/* whitespace -- keep looping */ ;

\\[^\n]*\n		/* end of line comment -- keep looping */ { STATE(yytext, "EOL comment"); }

-?{hex}+		{ token.type = TOK_NUMBER; token.text = yytext;
					return &token; }

\'.\'		{ token.type = TOK_C_LIT; token.text = yytext; return &token; }

\"{white}*(\\\"|[^"])*\"	{ token.type = TOK_STRING_LIT; token.text = yytext; 
				return &token; } /* String started by `"' or `."' */

\.\({white}*(\\\"|[^)])*\)	{ token.type = TOK_PSTRING; token.text = yytext; 
				return &token; } /* String of type `.(.....)' */

\.\"{white}*(\\\"|[^"])*\"	{ token.type = TOK_PSTRING; token.text = yytext; 
				return &token; }

"("		{ token.type = TOK_COMMENT; token.text = yytext;
				return &token; }

")"		{ token.type = TOK_ENDCOMMENT; token.text = yytext;
				return &token; }

":"		{ token.type = TOK_COLON; token.text = yytext;
				return &token; }

";"		{ token.type = TOK_SEMICOLON; token.text = yytext;
				return &token; }

\'		{ token.type = TOK_TOKENIZE; token.text = yytext;
				return &token; }

[aA][gG][aA][iI][nN]	{ token.type = TOK_AGAIN; token.text = yytext;
				return &token; }

[aA][lL][iI][aA][sS]	{ token.type = TOK_ALIAS; token.text = yytext;
				return &token; }

\[\'\]			{ token.type = TOK_GETTOKEN; token.text = yytext;
				return &token; }

[aA][sS][cC][iI][iI]	{ token.type = TOK_ASCII; token.text = yytext;
				return &token; }

[bB][eE][gG][iI][nN]	{ token.type = TOK_BEGIN; token.text = yytext;
				return &token; }

[bB][uU][fF][fF][eE][rR]:	{ token.type = TOK_BUFFER; token.text = yytext;
				return &token; }

[cC][aA][sS][eE]	{ token.type = TOK_CASE; token.text = yytext;
				return &token; }

[cC][oO][nN][sS][tT][aA][nN][tT]	{ token.type = TOK_CONSTANT; token.text = yytext;
				return &token; }

[cC][oO][nN][tT][rR][oO][lL]	{ token.type = TOK_CONTROL; token.text = yytext;
				return &token; }

[cC][rR][eE][aA][tT][eE]	{ token.type = TOK_CREATE; token.text = yytext;
				return &token; }

[dD]#		{ token.type = TOK_DECIMAL; token.text = yytext;
				return &token; }

[dD][eE][cC][iI][mM][aA][lL]	{ token.type = TOK_DECIMAL; token.text = yytext;
				return &token; }

[dD][eE][fF][eE][rR]	{ token.type = TOK_DEFER; token.text = yytext;
				return &token; }

\??[dD][oO]	{ token.type = TOK_DO; token.text = yytext;
				return &token; }

[eE][lL][sS][eE]	{ token.type = TOK_ELSE; token.text = yytext;
				return &token; }

[eE][nN][dD][cC][aA][sS][eE]	{ token.type = TOK_ENDCASE; token.text = yytext;
				return &token; }

[eE][nN][dD][oO][fF]	{ token.type = TOK_ENDOF; token.text = yytext;
				return &token; }

[eE][xX][tT][eE][rR][nN][aA][lL]	{ token.type = TOK_EXTERNAL; token.text = yytext;
				return &token; }

[fF][iI][eE][lL][dD]	{ token.type = TOK_FIELD; token.text = yytext;
				return &token; }

[hH]#		{ token.type = TOK_HEX; token.text = yytext;
				return &token; }

[hH][eE][aA][dD][eE][rR][lL][eE][sS][sS]	{ token.type = TOK_HEADERLESS; token.text = yytext;
				return &token; }

[hH][eE][aA][dD][eE][rR][sS]	{ token.type = TOK_HEADERS; token.text = yytext;
				return &token; }

[hH][eE][xX]	{ token.type = TOK_HEX; token.text = yytext;
				return &token; }

[iI][fF]		{ token.type = TOK_IF; token.text = yytext;
				return &token; }

\??[lL][eE][aA][vV][eE]	{ token.type = TOK_LEAVE; token.text = yytext;
				return &token; }

\+?[lL][oO][oO][pP]	{ token.type = TOK_LOOP; token.text = yytext;
				return &token; }

[oO]#		{ token.type = TOK_OCTAL; token.text = yytext;
				return &token; }

[oO][cC][tT][aA][lL]	{ token.type = TOK_OCTAL; token.text = yytext;
				return &token; }

[oO][fF]		{ token.type = TOK_OF; token.text = yytext;
				return &token; }

[rR][eE][pP][eE][aA][tT]	{ token.type = TOK_REPEAT; token.text = yytext;
				return &token; }

[tT][hH][eE][nN]	{ token.type = TOK_THEN; token.text = yytext;
				return &token; }

[tT][oO]		{ token.type = TOK_TO; token.text = yytext;
				return &token; }

[uU][nN][tT][iI][lL]	{ token.type = TOK_UNTIL; token.text = yytext;
				return &token; }

[vV][aA][lL][uU][eE]	{ token.type = TOK_VALUE; token.text = yytext;
				return &token; }

[vV][aA][rR][iI][aA][bB][lL][eE]	{ token.type = TOK_VARIABLE; token.text = yytext;
				return &token; }

[wW][hH][iI][lL][eE]	{ token.type = TOK_WHILE; token.text = yytext;
				return &token; }

offset16		{ token.type = TOK_OFFSET16; token.text = yytext;
				return &token; }

tokenizer\[	{ token.type = TOK_BEGTOK; token.text = yytext;
				return &token; }

emit-byte		{ token.type = TOK_EMIT_BYTE; token.text = yytext;
				return &token; }

\]tokenizer	{ token.type = TOK_ENDTOK; token.text = yytext;
				return &token; }

fload		{ token.type = TOK_FLOAD; token.text = yytext;
				return &token; }


[^ \n\t\r\f]+	{ token.type = TOK_OTHER; token.text = yytext;
				return &token; }

<<EOF>>			{ return NULL; }
%%

/* Function definitions */
void push(Cell);
Cell pop(void);
int depth(void);
int fadd(struct fcode *, struct fcode *);
struct fcode *flookup(struct fcode *, char *);
int aadd(struct macro *, struct macro *);
struct macro *alookup(struct macro *, char *);
void initdic(void);
void usage(char *);
void tokenize(YY_BUFFER_STATE);
int emit(char *);
int spit(long);
void sspit(char *);
int apply_macros(YY_BUFFER_STATE, char *);
int main(int argc, char *argv[]);

/*
 * Standard FCode names and numbers.  Includes standard
 * tokenizer aliases.
 */
struct fcode fcodes[] = {
		{ "end0",			0x0000 },
		{ "b(lit)",			0x0010 },
		{ "b(')",			0x0011 },
		{ "b(\")",			0x0012 },
		{ "bbranch",			0x0013 },
		{ "b?branch",			0x0014 },
		{ "b(loop)",			0x0015 },
		{ "b(+loop)",			0x0016 },
		{ "b(do)",			0x0017 },
		{ "b(?do)",			0x0018 },
		{ "i",				0x0019 },
		{ "j",				0x001a },
		{ "b(leave)",			0x001b },
		{ "b(of)",			0x001c },
		{ "execute",			0x001d },
		{ "+",				0x001e },
		{ "-",				0x001f },
		{ "*",				0x0020 },
		{ "/",				0x0021 },
		{ "mod",			0x0022 },
		{ "and",			0x0023 },
		{ "or",				0x0024 },
		{ "xor",			0x0025 },
		{ "invert",			0x0026 },
		{ "lshift",			0x0027 },
		{ "rshift",			0x0028 },
		{ ">>a",			0x0029 },
		{ "/mod",			0x002a },
		{ "u/mod",			0x002b },
		{ "negate",			0x002c },
		{ "abs",			0x002d },
		{ "min",			0x002e },
		{ "max",			0x002f },
		{ ">r",				0x0030 },
		{ "r>",				0x0031 },
		{ "r@",				0x0032 },
		{ "exit",			0x0033 },
		{ "0=",				0x0034 },
		{ "0<>",			0x0035 },
		{ "0<",				0x0036 },
		{ "0<=",			0x0037 },
		{ "0>",				0x0038 },
		{ "0>=",			0x0039 },
		{ "<",				0x003a },
		{ ">",				0x003b },
		{ "=",				0x003c },
		{ "<>",				0x003d },
		{ "u>",				0x003e },
		{ "u<=",			0x003f },
		{ "u<",				0x0040 },
		{ "u>=",			0x0041 },
		{ ">=",				0x0042 },
		{ "<=",				0x0043 },
		{ "between",			0x0044 },
		{ "within",			0x0045 },
		{ "drop",			0x0046 },
		{ "dup",			0x0047 },
		{ "over",			0x0048 },
		{ "swap",			0x0049 },
		{ "rot",			0x004a },
		{ "-rot",			0x004b },
		{ "tuck",			0x004c },
		{ "nip",			0x004d },
		{ "pick",			0x004e },
		{ "roll",			0x004f },
		{ "?dup",			0x0050 },
		{ "depth",			0x0051 },
		{ "2drop",			0x0052 },
		{ "2dup",			0x0053 },
		{ "2over",			0x0054 },
		{ "2swap",			0x0055 },
		{ "2rot",			0x0056 },
		{ "2/",				0x0057 },
		{ "u2/",			0x0058 },
		{ "2*",				0x0059 },
		{ "/c",				0x005a },
		{ "/w",				0x005b },
		{ "/l",				0x005c },
		{ "/n",				0x005d },
		{ "ca+",			0x005e },
		{ "wa+",			0x005f },
		{ "la+",			0x0060 },
		{ "na+",			0x0061 },
		{ "char+",			0x0062 },
		{ "wa1+",			0x0063 },
		{ "la1+",			0x0064 },
		{ "cell+",			0x0065 },
		{ "chars",			0x0066 },
		{ "/w*",			0x0067 },
		{ "/l*",			0x0068 },
		{ "cells",			0x0069 },
		{ "on",				0x006a },
		{ "off",			0x006b },
		{ "+!",				0x006c },
		{ "@",				0x006d },
		{ "l@",				0x006e },
		{ "w@",				0x006f },
		{ "<w@",			0x0070 },
		{ "c@",				0x0071 },
		{ "!",				0x0072 },
		{ "l!",				0x0073 },
		{ "w!",				0x0074 },
		{ "c!",				0x0075 },
		{ "2@",				0x0076 },
		{ "2!",				0x0077 },
		{ "move",			0x0078 },
		{ "fill",			0x0079 },
		{ "comp",			0x007a },
		{ "noop",			0x007b },
		{ "lwsplit",			0x007c },
		{ "wjoin",			0x007d },
		{ "lbsplit",			0x007e },
		{ "bljoin",			0x007f },
		{ "wbflip",			0x0080 },
		{ "upc",			0x0081 },
		{ "lcc",			0x0082 },
		{ "pack",			0x0083 },
		{ "count",			0x0084 },
		{ "body>",			0x0085 },
		{ ">body",			0x0086 },
		{ "fcode-revision",		0x0087 },
		{ "span",			0x0088 },
		{ "unloop",			0x0089 },
		{ "expect",			0x008a },
		{ "alloc-mem",			0x008b },
		{ "free-mem",			0x008c },
		{ "key?",			0x008d },
		{ "key",			0x008e },
		{ "emit",			0x008f },
		{ "type",			0x0090 },
		{ "(cr",			0x0091 },
		{ "cr",				0x0092 },
		{ "#out",			0x0093 },
		{ "#line",			0x0094 },
		{ "hold",			0x0095 },
		{ "<#",				0x0096 },
		{ "u#>",			0x0097 },
		{ "sign",			0x0098 },
		{ "u#",				0x0099 },
		{ "u#s",			0x009a },
		{ "u.",				0x009b },
		{ "u.r",			0x009c },
		{ ".",				0x009d },
		{ ".r",				0x009e },
		{ ".s",				0x009f },
		{ "base",			0x00a0 },
		{ "convert",			0x00a1 },
		{ "$number",			0x00a2 },
		{ "digit",			0x00a3 },
		{ "-1",				0x00a4 },
		{ "true",			0x00a4 },
		{ "0",				0x00a5 },
		{ "1",				0x00a6 },
		{ "2",				0x00a7 },
		{ "3",				0x00a8 },
		{ "bl",				0x00a9 },
		{ "bs",				0x00aa },
		{ "bell",			0x00ab },
		{ "bounds",			0x00ac },
		{ "here",			0x00ad },
		{ "aligned",			0x00ae },
		{ "wbsplit",			0x00af },
		{ "bwjoin",			0x00b0 },
		{ "b(<mark)",			0x00b1 },
		{ "b(>resolve)",		0x00b2 },
		{ "set-token-table",		0x00b3 },
		{ "set-table",			0x00b4 },
		{ "new-token",			0x00b5 },
		{ "named-token",		0x00b6 },
		{ "b(:)",			0x00b7 },
		{ "b(value)",			0x00b8 },
		{ "b(variable)",		0x00b9 },
		{ "b(constant)",		0x00ba },
		{ "b(create)",			0x00bb },
		{ "b(defer)",			0x00bc },
		{ "b(buffer:)",			0x00bd },
		{ "b(field)",			0x00be },
		{ "b(code)",			0x00bf },
		{ "instance",			0x00c0 },
		{ "b(;)",			0x00c2 },
		{ "b(to)",			0x00c3 },
		{ "b(case)",			0x00c4 },
		{ "b(endcase)",			0x00c5 },
		{ "b(endof)",			0x00c6 },
		{ "#",				0x00c7 },
		{ "#s",				0x00c8 },
		{ "#>",				0x00c9 },
		{ "external-token",		0x00ca },
		{ "$find",			0x00cb },
		{ "offset16",			0x00cc },
		{ "evaluate",			0x00cd },
		{ "c,",				0x00d0 },
		{ "w,",				0x00d1 },
		{ "l,",				0x00d2 },
		{ "'",				0x00d3 },
		{ "um*",			0x00d4 },
		{ "um/mod",			0x00d5 },
		{ "d+",				0x00d8 },
		{ "d-",				0x00d9 },
		{ "get-token",			0x00da },
		{ "set-token",			0x00db },
		{ "state",			0x00dc },
		{ "compile,",			0x00dd },
		{ "behavior",			0x00de },
		{ "start0",			0x00f0 },
		{ "start1",			0x00f1 },
		{ "start2",			0x00f2 },
		{ "start4",			0x00f3 },
		{ "ferror",			0x00fc },
		{ "version1",			0x00fd },
		{ "4-byte-id",			0x00fe },
		{ "end1",			0x00ff },
		{ "dma-alloc",			0x0101 },
		{ "my-address",			0x0102 },
		{ "my-space",			0x0103 },
		{ "memmap",			0x0104 },
		{ "free-virtual",		0x0105 },
		{ ">physical",			0x0106 },
		{ "my-params",			0x010f },
		{ "property",			0x0110 },
		{ "encode-int",			0x0111 },
		{ "encode+",			0x0112 },
		{ "encode-phys",		0x0113 },
		{ "encode-string",		0x0114 },
		{ "encode-bytes",		0x0115 },
		{ "reg",			0x0116 },
		{ "intr",			0x0117 },
		{ "driver",			0x0118 },
		{ "model",			0x0119 },
		{ "device-type",		0x011a },
		{ "parse-2int",			0x011b },
		{ "is-install",			0x011c },
		{ "is-remove",			0x011d },
		{ "is-selftest",		0x011e },
		{ "new-device",			0x011f },
		{ "diagnostic-mode?",		0x0120 },
		{ "display-status",		0x0121 },
		{ "memory-test-suite",		0x0122 },
		{ "group-code",			0x0123 },
		{ "mask",			0x0124 },
		{ "get-msecs",			0x0125 },
		{ "ms",				0x0126 },
		{ "find-device",		0x0127 },
		{ "decode-phys",		0x0128 },
		{ "map-low",			0x0130 },
		{ "sbus-intr>cpu",		0x0131 },
		{ "#lines",			0x0150 },
		{ "#columns",			0x0151 },
		{ "line#",			0x0152 },
		{ "column#",			0x0153 },
		{ "inverse?",			0x0154 },
		{ "inverse-screen?",		0x0155 },
		{ "frame-buffer-busy?",		0x0156 },
		{ "draw-character",		0x0157 },
		{ "reset-screen",		0x0158 },
		{ "toggle-cursor",		0x0159 },
		{ "erase-screen",		0x015a },
		{ "blink-screen",		0x015b },
		{ "invert-screen",		0x015c },
		{ "insert-characters",		0x015d },
		{ "delete-characters",		0x015e },
		{ "insert-lines",		0x015f },
		{ "delete-lines",		0x0160 },
		{ "draw-logo",			0x0161 },
		{ "frame-buffer-addr",		0x0162 },
		{ "screen-height",		0x0163 },
		{ "screen-width",		0x0164 },
		{ "window-top",			0x0165 },
		{ "window-left",		0x0166 },
		{ "default-font",		0x016a },
		{ "set-font",			0x016b },
		{ "char-height",		0x016c },
		{ "char-width",			0x016d },
		{ ">font",			0x016e },
		{ "fontbytes",			0x016f },
		{ "fb8-draw-character",		0x0180 },
		{ "fb8-reset-screen",		0x0181 },
		{ "fb8-toggle-cursor",		0x0182 },
		{ "fb8-erase-screen",		0x0183 },
		{ "fb8-blink-screen",		0x0184 },
		{ "fb8-invert-screen",		0x0185 },
		{ "fb8-insert-characters",	0x0186 },
		{ "fb8-delete-characters",	0x0187 },
		{ "fb8-inisert-lines",		0x0188 },
		{ "fb8-delete-lines",		0x0189 },
		{ "fb8-draw-logo",		0x018a },
		{ "fb8-install",		0x018b },
		{ "return-buffer",		0x01a0 },
		{ "xmit-packet",		0x01a1 },
		{ "poll-packet",		0x01a2 },
		{ "mac-address",		0x01a4 },
		{ "device-name",		0x0201 },
		{ "my-args",			0x0202 },
		{ "my-self",			0x0203 },
		{ "find-package",		0x0204 },
		{ "open-package",		0x0205 },
		{ "close-package",		0x0206 },
		{ "find-method",		0x0207 },
		{ "call-package",		0x0208 },
		{ "$call-parent",		0x0209 },
		{ "my-parent",			0x020a },
		{ "ihandle>phandle",		0x020b },
		{ "my-unit",			0x020d },
		{ "$call-method",		0x020e },
		{ "$open-package",		0x020f },
		{ "processor-type",		0x0210 },
		{ "firmware-version",		0x0211 },
		{ "fcode-version",		0x0212 },
		{ "alarm",			0x0213 },
		{ "(is-user-word)",		0x0214 },
		{ "suspend-fcode",		0x0215 },
		{ "abort",			0x0216 },
		{ "catch",			0x0217 },
		{ "throw",			0x0218 },
		{ "user-abort",			0x0219 },
		{ "get-my-property",		0x021a },
		{ "decode-int",			0x021b },
		{ "decode-string",		0x021c },
		{ "get-inherited-property",	0x021d },
		{ "delete-property",		0x021e },
		{ "get-package-property",	0x021f },
		{ "cpeek",			0x0220 },
		{ "wpeek",			0x0221 },
		{ "lpeek",			0x0222 },
		{ "cpoke",			0x0223 },
		{ "wpoke",			0x0224 },
		{ "lpoke",			0x0225 },
		{ "lwflip",			0x0226 },
		{ "lbflip",			0x0227 },
		{ "lbflips",			0x0228 },
		{ "adr-mask",			0x0229 },
		{ "rb@",			0x0230 },
		{ "rb!",			0x0231 },
		{ "rw@",			0x0232 },
		{ "rw!",			0x0233 },
		{ "rl@",			0x0234 },
		{ "rl!",			0x0235 },
		{ "wbflips",			0x0236 },
		{ "lwflips",			0x0237 },
		{ "probe",			0x0238 },
		{ "probe-virtual",		0x0239 },
		{ "child",			0x023b },
		{ "peer",			0x023c },
		{ "next-property",		0x023d },
		{ "byte-load",			0x023e },
		{ "set-args",			0x023f },
		{ "left-parse-string",		0x0240 },
			/* 64-bit FCode extensions */
		{ "bxjoin",			0x0241 },
		{ "<l@",			0x0242 },
		{ "lxjoin",			0x0243 },
		{ "rx@",			0x022e },
		{ "rx!",			0x022f },
		{ "wxjoin",			0x0244 },
		{ "x,",				0x0245 },
		{ "x@",				0x0246 },
		{ "x!",				0x0247 },
		{ "/x",				0x0248 },
		{ "/x*",			0x0249 },
		{ "xa+",			0x024a },
		{ "xa1+",			0x024b },
		{ "xbflip",			0x024c },
		{ "xbflips",			0x024d },
		{ "xbsplit",			0x024e },
		{ "xlflip",			0x024f },
		{ "xlflips",			0x0250 },
		{ "xlsplit",			0x0251 },
		{ "xwflip",			0x0252 },
		{ "xwflips",			0x0253 },
		{ "xwsplit",			0x0254 },
		{ NULL, 0 }
};

/*
 * Default macros -- can be overridden by colon definitions.
 */
struct macro macros[] = {
	{ "eval",	"evaluate" }, /* Build a more balanced tree */
	{ "(.)",	"dup abs <# u#s swap sign u#>" },
	{ "<<",		"lshift" },
	{ ">>",		"rshift" },
	{ "?",		"@ ." },
	{ "1+",		"1 +" },
	{ "1-",		"1 -" },
	{ "2+",		"2 +" },
	{ "2-",		"2 -" },
	{ "abort\"",	"-2 throw" },
	{ "accept",	"span @ -rot expect span @ swap span !" },
	{ "allot",	"0 max 0 ?do 0 c, loop" },
	{ "blank",	"bl fill" },
	{ "/c*",	"chars" },
	{ "ca1+",	"char+" },
	{ "carret",	"b(lit) 00 00 00 0x0d" },
	{ ".d"		"base @ swap 0x0a base ! . base !" },
	{ "decode-bytes", ">r over r@ + swap r@ - rot r>" },
	{ "3drop",	"drop 2drop" },
	{ "3dup",	"2 pick 2 pick 2 pick" },
	{ "erase",	"0 fill" },
	{ "false",	"0" },
	{ ".h"		"base @ swap 0x10 base ! . base !" },
	{ "linefeed",	"b(lit) 00 00 00 0x0a" },
	{ "/n*",	"cells" },
	{ "na1+",	"cell+", },
	{ "not",	"invert", },
	{ "s.",		"(.) type space" },
	{ "space",	"bl emit" },
	{ "spaces",	"0 max 0 ?do space loop" },
	{ "struct",	"0" },
	{ "true",	"-1" },
	{ "(u,)",	"<# u#s u#>" },
	{ NULL, NULL }
};

/*
 * Parser stack control functions.
 */

void
push(val)
Cell val;
{
	parse_stack[parse_stack_ptr++] = val;
	if (parse_stack_ptr >= PSTKSIZ) {
		(void)printf( "Parse stack overflow\n");
		exit(1);
	}
}

Cell
pop()
{
	ASSERT(parse_stack_ptr);
	return parse_stack[--parse_stack_ptr];
}

int
depth()
{
	return (parse_stack_ptr);
}

/*
 * Insert fcode into dictionary.
 */
int
fadd(dict, new)
struct fcode *dict, *new;
{
	int res = strcmp(dict->name, new->name);

#ifdef DEBUG
	new->type = FCODE;
	ASSERT(dict->type == FCODE);
#endif
	/* Don't allow duplicate entries. */
	if (!res) return (0);
	if (res < 0) {
		if (dict->l)
			return fadd(dict->l, new);
		else {
#ifdef DEBUG
			if (debug > 1)
				(void)printf( "fadd: new FCode `%s' is %lx\n", 
					      new->name, new->num);
#endif
			new->l = new->r = NULL;
			dict->l = new;
		}
	} else {
		if (dict->r)
			return fadd(dict->r, new);
		else {
#ifdef DEBUG
			if (debug > 1)
				(void)printf( "fadd: new FCode `%s' is %lx\n", 
					      new->name, new->num);
#endif
			new->l = new->r = NULL;
			dict->r = new;
		}
	}
	return (1);
}

/*
 * Look for a code in the dictionary.
 */
struct fcode *
flookup(dict, str)
struct fcode *dict;
char *str;
{
	int res;
	if (!dict) return (dict);

	res = strcmp(dict->name, str);
#ifdef DEBUG
	ASSERT(dict->type == FCODE);
	if (debug > 2)
		(void)printf( "flookup: `%s' and `%s' %s match\n", 
			      str, dict->name, res?"don't":"do");
#endif
	if (!res) return (dict);
	if (res < 0)
		return (flookup(dict->l, str));
	else 
		return (flookup(dict->r, str));

}

/*
 * Insert alias into macros.
 */
int
aadd(dict, new)
	struct macro *dict, *new;
{
	int res = strcmp(dict->name, new->name);

#ifdef DEBUG
	new->type = MACRO;
	ASSERT(dict->type == MACRO);
#endif
	/* Don't allow duplicate entries. */
	if (!res) return (0);
	if (res < 0) {
		if (dict->l)
			return aadd(dict->l, new);
		else {
			new->l = new->r = NULL;
			dict->l = new;
#ifdef DEBUG
			if (debug > 1)
				(void)printf( "aadd: new alias `%s' to `%s'\n", 
					      new->name, new->equiv);
#endif
		}
	} else {
		if (dict->r)
			return aadd(dict->r, new);
		else {
			new->l = new->r = NULL;
			dict->r = new;
#ifdef DEBUG
			if (debug > 1)
				(void)printf( "aadd: new alias `%s' to `%s'\n", 
					      new->name, new->equiv);
#endif
		}
	}
	return (1);
}

/*
 * Look for a macro in the aliases.
 */
struct macro *
alookup(dict, str)
struct macro *dict;
char *str;
{
	int res;
	if (!dict) return (dict);

#ifdef DEBUG
	ASSERT(dict->type == MACRO);
#endif
	res = strcmp(dict->name, str);
	if (!res) return (dict);
	if (res < 0)
		return (alookup(dict->l, str));
	else 
		return (alookup(dict->r, str));

}

/*
 * Bootstrap the dictionary and then install
 * all the standard FCodes.
 */
void
initdic()
{
	struct fcode *code = fcodes;
	struct macro *alias = macros;

	ASSERT(dictionary == NULL);
	code->l = code->r = NULL;
	dictionary = code;
#ifdef DEBUG
	code->type = FCODE;
#endif

	while ((++code)->name) {
		if(!fadd(dictionary, code)) {
			printf("init: duplicate dictionary entry %s\n", 
			       code->name);
			abort();
		}
	}

	ASSERT(aliases == NULL);
	aliases = alias;
	alias->l = alias->r = NULL;
#ifdef DEBUG
	alias->type = MACRO;
#endif
	while ((++alias)->name) {
		if(!aadd(aliases, alias)) {
			printf("init: duplicate macro entry %s\n", 
			       alias->name);
			abort();
		}
	}

}

int
apply_macros(input, str) 
	YY_BUFFER_STATE input;
	char *str;
{
	struct macro *xform = alookup(aliases, str);
	
	if (xform) {
		YY_BUFFER_STATE newbuf;

		newbuf = yy_scan_string(xform->equiv);
		yy_switch_to_buffer(newbuf);
		tokenize(newbuf);
		yy_switch_to_buffer(input);
		yy_delete_buffer(newbuf);
	}
	return (xform != NULL);
}

void
usage(me)
	char *me;
{
	(void)fprintf(stderr, "usage: %s [-d level] [-o outfile] infile\n", me);
	exit(1);
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int bflag, ch;
	FILE *inf;
	struct fcode_header *fheader;
	YY_BUFFER_STATE inbuf;
	char *hdrtype = "version1";
	int i;

	outf = 1; /* stdout */
	myname = argv[0];

	bflag = 0;
	while ((ch = getopt(argc, argv, "d:o:")) != -1)
		switch(ch) {
		case 'd':
			debug = atol(optarg);
			break;
		case 'o':
			outfile = optarg;
			break;
		case '?':
		default:
			warnx("Illegal argument: %c", ch);
			usage(myname);
		}
	argc -= optind;
	argv += optind;
	
	if (argc != 1)
		usage(myname);
	
	infile = argv[0];

	/*
	 * Initialization stuff.
	 */
	initdic();
	outbufsiz = BUFCLICK;
	outbuf = malloc(outbufsiz);
	if (outbuf == NULL)
		(void)err(1, "out of memory");

	fheader = (struct fcode_header *)outbuf;
	outpos = 0;
	emit(hdrtype);
	outpos = sizeof(*fheader);

	/* 
	 * Do it.
	 */
	if ((inf = fopen(infile, "r")) == NULL)
		(void)err(1, "can not open %s for reading", infile);

	inbuf = yy_create_buffer( inf, YY_BUF_SIZE );
	yy_switch_to_buffer(inbuf);
	tokenize(inbuf);
	yy_delete_buffer(inbuf);
	fclose(inf);
	emit("end0");

	/* Now calculate length and checksum and stick them in the header */
	fheader->format = 0x08;
	fheader->length = htonl(outpos);
	fheader->checksum = 0;
	for (i = sizeof(*fheader); i<outpos; i++)
		fheader->checksum += outbuf[i];
	fheader->checksum = htons(fheader->checksum);

	if ((outf = open(outfile, O_WRONLY|O_CREAT|O_TRUNC, 0666)) == -1)
		err(1, "can out open %s for writing", outfile);

	if (write(outf, outbuf, outpos) != outpos) {
		close(outf);
		unlink(outfile);
		err(1, "write error");
	}
	close(outf);
	return (0);
};

/*
 * Tokenize one file.  This is a separate function so it can
 * be called recursively to parse multiple levels of include files.
 */

void
tokenize(input) 
	YY_BUFFER_STATE input;
{
	FILE *inf;
	YY_BUFFER_STATE inbuf;
	TOKEN *token;
	char *last_token = "";
	struct fcode *fcode;
	int pos, off;

	while ((token = yylex()) != NULL) {
		switch (token->type) {
		case TOK_NUMBER:
			STATE(token->text, "TOK_NUMBER");
		{ 
			char *end;
			Cell value;
			
			if (tokenizer) {
				push(strtol(token->text, &end, 16));
				break;
			}
			value = strtol(token->text, &end, base);
			if (*end != 0)
				token_err(yylineno, infile, yytext,
				    "illegal number conversion");

			/* 
			 * If this is a 64-bit value we need to store two literals
			 * and issue a `lxjoin' to combine them.  But that's a future
			 * project.
			 */
			emit("b(lit)");
			spit(value>>24);
			spit((value>>16)&0x0ff);
			spit((value>>8)&0x0ff);
			spit(value&0x0ff);
		}
		break;
		case TOK_C_LIT:
			STATE(token->text, "TOK_C_LIT");
			emit("b(lit)");
			spit(0);
			spit(0);
			spit(0);
			spit(token->text[1]);
		break;
		case TOK_STRING_LIT: 
			STATE(token->text, "TOK_STRING_LIT:");
		{
			int len;
			char *p = token->text;
			
			++p;			/* Skip the quote */
			len = strlen(++p);	/* Skip the 1st space */

#define ERR_TOOLONG	\
	token_err(yylineno, infile, yytext, "string length %d too long", len)

			if (len > 255)
				ERR_TOOLONG;

			if (p[len-1] == ')' ||
			    p[len-1] == '"') {
				p[len-1] = 0;
			}
			emit("b(\")");
			sspit(p);
		}
		break;
		case TOK_PSTRING: 
			STATE(token->text, "TOK_PSTRING:");
		{
			int len;
			char *p = token->text;

			if (*p++ == '.') p++; /* Skip over delimiter */
			p++; /* Skip over space/tab */

			len = strlen(p);
			if (len > 255)
				ERR_TOOLONG;

			if (p[len-1] == ')' ||
			    p[len-1] == '"') {
				p[len-1] = 0;
			}
			emit("b(\")");
			sspit(p);
			emit("type");
		}
		break;
		case TOK_TOKENIZE:
			STATE(token->text, "TOK_TOKENIZE");
			/* The next pass should tokenize the FCODE number */
			emit("b(')");
			break;
		case TOK_COMMENT: 
			STATE(token->text, "TOK_COMMENT:");
			while (((token = yylex()) != NULL) && token->type != TOK_ENDCOMMENT)
				;
			break;
		case TOK_ENDCOMMENT:
			STATE(token->text, "TOK_ENDCOMMENT");
			token_err(yylineno, infile, NULL,
			    "ENDCOMMENT encountered outside comment");
			break;
		case TOK_COLON: 
			STATE(token->text, "TOK_COLON:");

			token = yylex();
			if (token == NULL)
				token_err(yylineno, infile, yytext,
				    "EOF in colon definition");
			
			/* Add new code to dictionary */
			fcode = malloc(sizeof(*fcode));
			if (fcode == NULL)
				(void)err(1, "out of memory");

			fcode->num = nextfcode++;
			fcode->name = strdup(token->text);
			if (fcode->name == NULL)
				(void)err(1, "out of memory");
			
			if (!fadd(dictionary, fcode))
				token_err(yylineno, infile, NULL,
				    "Duplicate definition: `%s'\n", fcode->name);
#ifdef DEBUG
			if (debug)
				(void)printf("Adding %s to dictionary\n", token->text);		
#endif
			if (state == 0)
				emit("new-token");
			else {
				if (state == TOK_EXTERNAL)
					emit("external-token");
				else
				/* Here we have a choice of new-token or named-token */
					emit("named-token");
				sspit(token->text);
			}
			spit(fcode->num);
			emit("b(:)");
			last_token = fcode->name;
			defining = 1;
 			break;
		case TOK_SEMICOLON: 
			STATE(token->text, "TOK_SEMICOLON:");
			emit("b(;)");
			defining = 0;
			if (depth()) {
				token_err(yylineno, infile, NULL,
				    "Warning: stack depth %d at end of %s\n",
				    depth(), last_token);
			}
			last_token = "";
			break;

			/* These are special */
		case TOK_AGAIN:
			STATE(token->text, "TOK_AGAIN");
			emit("bbranch");
			pos = pop();
			pos -= outpos;
			if (offsetsize == 16) {
				spit((pos>>8)&0xff);
			}
			spit(pos&0xff);
			break;
		case TOK_ALIAS:
			STATE(token->text, "TOK_ALIAS");
		{
			struct macro *alias;

			token = yylex();
			if (token == NULL) {
				(void)printf( "EOF in alias definition\n");
				return;
			}
			if (token->type != TOK_OTHER) {
				(void)printf( "ENDCOMMENT aliasing weird token type %d\n",
					      token->type);
			}
			alias = malloc(sizeof(*alias));
			if (alias == NULL)
				(void)err(1, "out of memory");

			alias->name = strdup(token->text);
			if (alias->name == NULL)
				(void)err(1, "out of memory");
			
			token = yylex();
			if (token == NULL) {
				free(alias);
				(void)printf( "EOF in alias definition\n");
				return;
			}			
			alias->equiv = strdup(token->text);
			if (alias->equiv == NULL)
				(void)err(1, "out of memory");
			
			if (!aadd(aliases, alias)) {
				(void)printf( "ERROR: Duplicate alias %s\n",
					      alias->name);
				exit(1);
			}
		}
		break;
		case TOK_GETTOKEN:
			STATE(token->text, "TOK_GETTOKEN");
			/* This is caused by ['] */
			emit("b(')");
			token = yylex();
			if (token == NULL) {
				(void)printf( "EOF in [']\n");
				return;
			}
			if ((fcode = flookup(dictionary, token->text)) == NULL) {
				(void)printf( "[']: %s not found\n", token->text);
				exit(1);
			}
			spit(fcode->num);
			break;
		case TOK_ASCII:
			STATE(token->text, "TOK_ASCII");
			token = yylex();
			if (token == NULL) {
				(void)printf( "EOF after \"ascii\"\n");
				exit(1);
			}
			emit("b(lit)");
			spit(0);
			spit(0);
			spit(0);
			spit(token->text[0]);
			break;
		case TOK_BEGIN:
			STATE(token->text, "TOK_BEGIN");
			emit("b(<mark)");
			push(outpos);
			break;
		case TOK_BUFFER:
			STATE(token->text, "TOK_BUFFER");

			token = yylex();
			if (token == NULL) {
				(void)printf( "EOF in colon definition\n");
				return;
			}
			
			/* Add new code to dictionary */
			fcode = malloc(sizeof(*fcode));
			if (fcode == NULL)
				(void)err(1, "out of memory");

			fcode->num = nextfcode++;
			fcode->name = strdup(token->text);
			if (fcode->name == NULL)
				(void)err(1, "out of memory");
			
			fadd(dictionary, fcode);
			
			if (state == 0)
				emit("new-token");
			else {
				if (state == TOK_EXTERNAL)
					emit("external-token");
				else
				/* Here we have a choice of new-token or named-token */
					emit("named-token");
				sspit(token->text);
			}
			spit(fcode->num);
			emit("b(buffer:)");
			break;
		case TOK_CASE:
			STATE(token->text, "TOK_CASE");
			emit("b(case)");
			push(0);
			break;
		case TOK_CONSTANT:
			STATE(token->text, "TOK_CONSTANT");

			token = yylex();
			if (token == NULL) {
				(void)printf( "EOF in constant definition\n");
				return;
			}
			
			/* Add new code to dictionary */
			fcode = malloc(sizeof(*fcode));
			if (fcode == NULL)
				(void)err(1, "out of memory");
			fcode->num = nextfcode++;
			fcode->name = strdup(token->text);
			if (fcode->name == NULL)
				(void)err(1, "out of memory");
			
			fadd(dictionary, fcode);
			
			if (state == 0)
				emit("new-token");
			else {
				if (state == TOK_EXTERNAL)
					emit("external-token");
				else
				/* Here we have a choice of new-token or named-token */
					emit("named-token");
				sspit(token->text);
			}
			spit(fcode->num);
			emit("b(constant)");
			break;
		case TOK_CONTROL:
			STATE(token->text, "TOK_CONTROL");
			token = yylex();
			if (token == NULL) {
				(void)printf( "EOF after \"ascii\"\n");
				exit(1);
			}
			emit("b(lit)");
			spit(0);
			spit(0);
			spit(0);
			spit(token->text[0]&0x1f);
			break;
		case TOK_CREATE:
			STATE(token->text, "TOK_CREATE");
			/* Don't know what this does or if it's right */
			token = yylex();
			if (token == NULL) {
				(void)printf( "EOF in create definition\n");
				return;
			}
			
			/* Add new code to dictionary */
			fcode = malloc(sizeof(*fcode));
			if (fcode == NULL)
				err(1, "out of memory");

			fcode->num = nextfcode++;
			fcode->name = strdup(token->text);
			if (fcode->name == NULL)
				(void)err(1, "out of memory");

			fadd(dictionary, fcode);
			
			if (state == 0)
				emit("new-token");
			else {
				if (state == TOK_EXTERNAL)
					emit("external-token");
				else
				/* Here we have a choice of new-token or named-token */
					emit("named-token");
				sspit(token->text);
			}
			spit(fcode->num);
			emit("b(create)");
			break;
		case TOK_DECIMAL:
			STATE(token->text, "TOK_DECIMAL");
			if (token->text[1] != '#') {
				if (defining) {
					spit(10);
					emit("base");
					emit("!");
				} else
					base = TOK_DECIMAL;
			} else {
				char *end;
				Cell value;

				token = yylex();
				if (token == NULL) {
					(void)printf( "EOF after d#\n");
					return;
				}
				if (token->type == TOK_OTHER) {
					if (strcmp("-1", token->text) == 0) {
						emit(token->text);
						break;
					}
				}
				value = strtol(token->text, &end, 10);
				if (*end != 0)
					token_err(yylineno, infile, NULL,
					    "Illegal number conversion: %s", token->text);

				/* 
				 * If this is a 64-bit value we need to store two literals
				 * and issue a `lxjoin' to combine them.  But that's a future
				 * project.
				 */
				emit("b(lit)");
				spit(value>>24);
				spit((value>>16)&0x0ff);
				spit((value>>8)&0x0ff);
				spit(value&0x0ff);
			}
			break;
		case TOK_DEFER:
			STATE(token->text, "TOK_DEFER");
			/* Don't know what this does or if it's right */
			token = yylex();
			if (token == NULL) {
				(void)printf( "EOF in colon definition\n");
				return;
			}
			
			/* Add new code to dictionary */
			fcode = malloc(sizeof(*fcode));
			if (fcode == NULL)
				(void)err(1, "out of memory");

			fcode->num = nextfcode++;
			fcode->name = strdup(token->text);
			if (fcode->name == NULL)
				(void)err(1, "out of memory");
			
			fadd(dictionary, fcode);
			
			if (state == 0)
				emit("new-token");
			else {
				if (state == TOK_EXTERNAL)
					emit("external-token");
				else
				/* Here we have a choice of new-token or named-token */
					emit("named-token");
				sspit(token->text);
			}
			spit(fcode->num);
			emit("b(defer)");
			break;
		case TOK_DO:
			STATE(token->text, "TOK_DO");
			/*
			 * From the 1275 spec.  B is branch location, T is branch target.
			 *
			 *	b(do)  offset1 ... b(loop)  offset2 ...
			 *	b(do)  offset1 ... b(+loop) offset2 ...
			 *	b(?do) offset1 ... b(loop)  offset2 ...
			 *	b(?do) offset1 ... b(+loop) offset2 ...
			 *            ^                            ^
			 *           B1       ^            ^       T1
			 *                    T2           B2
			 *
			 * How we do this is we generate the b(do) or b(?do), spit out a
			 * zero offset while remembering b1 and t2.  Then we call tokenize()
			 * to generate the body.  When tokenize() finds a b(loop) or b(+loop),
			 * it generates the FCode and returns, with outpos at b2.  We then
			 * calculate the offsets, put them in the right slots and finishup.
			 */
			
			if (token->text[0] == '?')
				emit("b(?do)");
			else
				emit("b(do)");
			push(outpos);
			if (offsetsize == 16) {
				spit(0);
			}
			spit(0);	/* Place holder for later */
			push(outpos);
			break;
		case TOK_ELSE:
			STATE(token->text, "TOK_ELSE");
			/* Get where we need to patch */
			off = pop();
			emit("bbranch");
			/* Save where we are now. */
			push(outpos);
			if (offsetsize == 16) {
				spit(0);	/* Place holder for later */
			}
			spit(0);	/* Place holder for later */
			emit("b(>resolve)");
			/* Rewind and patch the if branch */
			pos = outpos;
			outpos = off;
			off = pos - off;
			if (offsetsize == 16) {
				spit(0);	/* Place holder for later */
			}
			spit(0);	/* Place holder for later */
			/* revert to the end */
			outpos = pos;
			break;
		case TOK_ENDCASE: 
			STATE(token->text, "TOK_ENDCASE:");
			pos = outpos; /* Remember where we need to branch to */

			/* Thread our way backwards and install proper offsets */
			off = pop();
			while (off) {
				int tmp;

				/* Move to this offset */
				outpos = off;
				/* Load next offset to process */
				tmp = outbuf[outpos];

				/* process this offset */
				off = pos - outpos;
				if (offsetsize == 16) {
					spit((off>>8)&0xff);
				}
				spit(off&0xff);
				off = tmp;
			}
			outpos = pos;
			emit("b(endcase)");
			break;
		case TOK_ENDOF:
			STATE(token->text, "TOK_ENDOF");
			off = pop();
			emit("b(endof)");
			/* 
			 * Save back pointer in the offset field so we can traverse
			 * the linked list and patch it in the endcase.
			 */
			pos = pop();	/* get position of prev link. */
			push(outpos);	/* save position of this link. */
			spit(pos);	/* save position of prev link. */
			if (offsetsize == 16) {
				spit(0);
			}
			pos = outpos;
			/* Now point the offset from b(of) here. */
			outpos = off;
			off = outpos - off;
			if (offsetsize == 16) {
				spit((off>>8)&0xff);
			}
			spit(off&0xff);
			/* Restore position */
			outpos = pos;
			break;
		case TOK_EXTERNAL:
			STATE(token->text, "TOK_EXTERNAL");
			state = TOK_EXTERNAL;
			break;
		case TOK_FIELD:
			STATE(token->text, "TOK_FIELD");
	
			token = yylex();
			if (token == NULL) {
				(void)printf( "EOF in field definition\n");
				return;
			}
			
			/* Add new code to dictionary */
			fcode = malloc(sizeof(*fcode));
			if (fcode == NULL)
				(void)err(1, "out of memory");

			fcode->num = nextfcode++;
			fcode->name = strdup(token->text);
			if (fcode->name == NULL)
				(void)err(1, "out of memory");
			
			fadd(dictionary, fcode);
			
			if (state == 0)
				emit("new-token");
			else {
				if (state == TOK_EXTERNAL)
					emit("external-token");
				else
				/* Here we have a choice of new-token or named-token */
					emit("named-token");
				sspit(token->text);
			}
			spit(fcode->num);
			emit("b(field)");
			break;
		
		case TOK_HEX:
			STATE(token->text, "TOK_HEX");
			if (token->text[1] != '#') {
				if (defining) {
					spit(16);
					emit("base");
					emit("!");
				} else
					base = TOK_HEX;
			} else {
				char *end;
				Cell value;

				token = yylex();
				if (token == NULL) {
					(void)printf( "EOF after h#\n");
					return;
				}
				value = strtol(token->text, &end, 16);
				if (*end != 0) {
					(void)printf("Illegal number conversion:%s:%d: %s\n",
					    infile, yylineno, yytext);
					exit(1);
				}
				/* 
				 * If this is a 64-bit value we need to store two literals
				 * and issue a `lxjoin' to combine them.  But that's a future
				 * project.
				 */
				emit("b(lit)");
				spit(value>>24);
				spit((value>>16)&0x0ff);
				spit((value>>8)&0x0ff);
				spit(value&0x0ff);
			}
			break;
		case TOK_HEADERLESS:
			STATE(token->text, "TOK_HEADERLESS");
			state = 0;
			break;
		case TOK_HEADERS:
			STATE(token->text, "TOK_HEADERS");
			state = TOK_HEADERS;
			break;
		case TOK_OFFSET16:
			STATE(token->text, "TOK_OFFSET16");
			offsetsize = 16;
			emit("offset16");
			break;
		case TOK_IF:
			STATE(token->text, "TOK_IF");
			/*
			 * Similar to do but simpler since we only deal w/one branch.
			 */
			emit("b?branch");
			push(outpos);
			if (offsetsize == 16) {
				spit(0);	/* Place holder for later */
			}
			spit(0);	/* Place holder for later */
			break;
		case TOK_LEAVE:
			STATE(token->text, "TOK_LEAVE");
			emit("b(leave)");
			break;
		case TOK_LOOP:
			STATE(token->text, "TOK_LOOP");

			if (token->text[0] == '+')
				emit("b(+loop)");
			else
				emit("b(loop)");
			/* First do backwards branch of loop */
			pos = pop();
			off = pos - outpos;
			if (offsetsize == 16) {
				spit((off>>8)&0xff);
			}
			spit(off&0xff);
			/* Now do forward branch of do */
			pos = outpos;
			outpos = pop();
			off = pos - outpos;
			if (offsetsize == 16) {
				spit((off>>8)&0xff);
			}
			spit(off&0xff);
			/* Restore output position */
			outpos = pos;
			break;
		case TOK_OCTAL:
			STATE(token->text, "TOK_OCTAL");
			if (token->text[1] != '#') {
				if (defining) {
					spit(16);
					emit("base");
					emit("!");
				} else
					base = TOK_OCTAL;
			} else {
				char *end;
				Cell value;

				token = yylex();
				if (token == NULL) {
					(void)printf( "EOF after o#\n");
					return;
				}
				value = strtol(token->text, &end, 8);
				if (*end != 0) {
					(void)printf("Illegal number conversion:%s:%d: %s\n",
					    infile, yylineno, yytext);
					exit(1);
				}
				/* 
				 * If this is a 64-bit value we need to store two literals
				 * and issue a `lxjoin' to combine them.  But that's a future
				 * project.
				 */
				emit("b(lit)");
				spit(value>>24);
				spit((value>>16)&0x0ff);
				spit((value>>8)&0x0ff);
				spit(value&0x0ff);
			}
			break;
		case TOK_OF:
			STATE(token->text, "TOK_OF");
			/*
			 * Let's hope I get the semantics right.
			 *
			 * The `of' behaves almost the same as an
			 * `if'.  The difference is that `endof'
			 * takes a branch offset to the associated
			 * `endcase'.  Here we will generate a temporary
			 * offset of the `of' associated with the `endof'.
			 * Then in `endcase' we should be pointing just
			 * after the offset of the last `endof' so we 
			 * calculate the offset and thread our way backwards
			 * searching for the previous `b(case)' or `b(endof)'.
			 */
			emit("b(of)");
			push(outpos);
			if (offsetsize == 16) {
				spit(0);
			}
			spit(0);	/* Place holder for later */
			break;
		case TOK_REPEAT:
			STATE(token->text, "TOK_REPEAT");
			emit("bbranch");
			pos = pop();
			off = pop();
			/* First the offset for the branch back to the begin */
			off -= outpos;
			if (offsetsize == 16) {
				spit((off>>8)&0xff);
			}
			spit(off&0xff);
			emit("b(>resolve)");
			/* Now point the offset of the while here. */
			off = outpos;
			outpos = pos;
			pos = off - pos;
			if (offsetsize == 16) {
				spit((pos>>8)&0xff);
			}
			spit(pos&0xff);
			/* Return to the end of the output */
			outpos = off;
			break;
		case TOK_THEN:
			STATE(token->text, "TOK_THEN");
			emit("b(>resolve)");
			pos = outpos;
			outpos = pop();
			off = pos - outpos;
			if (offsetsize == 16) {
				spit((off>>8)&0xff);
			}
			spit(off&0xff);
			outpos = pos;
			break;
		case TOK_TO:
			STATE(token->text, "TOK_TO");
			/* The next pass should tokenize the FCODE number */
			emit("b(to)");
			break;
		case TOK_UNTIL:
			STATE(token->text, "TOK_UNTIL");
		{
			int pos;

			emit("b?branch");
			pos = pop();
			pos -= outpos;
			if (offsetsize == 16) {
				spit((pos>>8)&0xff);
			}
			spit(pos&0xff);
		}
		break;			
		case TOK_VALUE:
			STATE(token->text, "TOK_VALUE");

			token = yylex();
			if (token == NULL) {
				(void)printf( "EOF in value definition\n");
				return;
			}
			
			/* Add new code to dictionary */
			fcode = malloc(sizeof(*fcode));
			if (fcode == NULL)
				(void)err(1, "out of memory");

			fcode->num = nextfcode++;
			fcode->name = strdup(token->text);
			if (fcode->name == NULL)
				(void)err(1, "out of memory");
			
			fadd(dictionary, fcode);
			
			if (state == 0)
				emit("new-token");
			else {
				if (state == TOK_EXTERNAL)
					emit("external-token");
				else
				/* Here we have a choice of new-token or named-token */
					emit("named-token");
				sspit(token->text);
			}
			spit(fcode->num);
			emit("b(value)");
			break;
		case TOK_VARIABLE:
			STATE(token->text, "TOK_VARIABLE");

			token = yylex();
			if (token == NULL) {
				(void)printf( "EOF in variable definition\n");
				return;
			}
			
			/* Add new code to dictionary */
			fcode = malloc(sizeof(*fcode));
			if (fcode == NULL)
				(void)err(1, "out of memory");

			fcode->num = nextfcode++;
			fcode->name = strdup(token->text);
			if (fcode->name == NULL)
				(void)err(1, "out of memory");
			
			fadd(dictionary, fcode);
			
			if (state == 0)
				emit("new-token");
			else {
				if (state == TOK_EXTERNAL)
					emit("external-token");
				else
				/* Here we have a choice of new-token or named-token */
					emit("named-token");
				sspit(token->text);
			}
			spit(fcode->num);
			emit("b(variable)");
			break;
		case TOK_WHILE:
			STATE(token->text, "TOK_WHILE");
			emit("b?branch");
			push(outpos);
			if (offsetsize == 16) {
				spit(0);
			}
			spit(0);
			break;

			/* Tokenizer directives */
		case TOK_BEGTOK:
			STATE(token->text, "TOK_BEGTOK");
			tokenizer = 1;
			break;
		case TOK_EMIT_BYTE:
			STATE(token->text, "TOK_EMIT_BYTE");
			spit(pop());
			break;
		case TOK_ENDTOK:
			STATE(token->text, "TOK_ENDTOK");
			tokenizer = 0;
			break;
		case TOK_FLOAD:
			STATE(token->text, "TOK_FLOAD");
			/* Parse a different file for a while */
			token = yylex();
			if ((inf = fopen(token->text, "r")) == NULL) {
				(void)printf("%s: Could not open %s: %s\n",
					      myname, token->text, strerror(errno));
				break;
			}
			inbuf = yy_create_buffer(inf, YY_BUF_SIZE);
			yy_switch_to_buffer(inbuf);
			{
				char *oldinfile = infile;

				infile = token->text;
				tokenize(inbuf);
				infile = oldinfile;
			}
			yy_switch_to_buffer(input);
			yy_delete_buffer(inbuf);
			fclose(inf);
			break;
		case TOK_OTHER:
			STATE(token->text, "TOK_OTHER");
			if (apply_macros(input, token->text))
				break;
			if (emit(token->text)) {
#if 0
				/*
				 * Call an external command 
				 *
				 * XXXXX assumes it will always find the command
				 */
				sspit(token->text);
				emit("$find");
				emit("drop");
				emit("execute");
#else
				(void)printf( "%s: undefined token `%s'\n",
					      myname, token->text);
				fflush(stderr);
				exit(1);
#endif
			}
			break;
		default:
			break;
		}
	}
	return;
}

/*
 * print a tokenizer error message
 */
void
token_err(int lineno, char *infile, char *text, char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (infile)
		(void)fprintf(stderr, "%s:%d: ", infile, lineno);
	if (fmt)
		(void)vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	if (text)
		fprintf(stderr, "\t%s", text);
	va_end(ap);
	exit(1);
}

/*
 * Lookup fcode string in dictionary and spit it out.
 *
 * Fcode must be in dictionary.  No alias conversion done.
 */
int
emit(str)
	char *str;
{
	struct fcode *code;
	if ((code = flookup( dictionary, str)))
		spit(code->num);
#ifdef DEBUG
	if (debug > 1) {
		if (code)
			(void)printf( "emitting `%s'\n", code->name);
		else
			(void)printf( "emit: not found `%s'\n", str);
	}
#endif
	return (code == NULL);
}

/*
 * Spit out an integral value as a series of FCodes.
 *
 * It will spit out one zero byte or as many bytes as are
 * non-zero.
 */
int
spit(n)
	long n;
{
	int count = 1;

	if (n >> 8)
		count += spit(n >> 8);
	if (outpos >= outbufsiz) {
		while (outpos >= outbufsiz) outbufsiz += BUFCLICK;
		if (!(outbuf = realloc(outbuf, outbufsiz))) {
			(void)printf( "realloc of %ld bytes failed -- out of memory\n",
				      (long)outbufsiz);
			exit(1);
		}
	}
	outbuf[outpos++] = n;
	return (count);
}

/*
 * Spit out an FCode string.
 */
void
sspit(s)
	char *s;
{
	int len = strlen(s);

	if (len > 255) {
		(void)printf( "string length %d too long\n", len);
		return;
	}
#ifdef DEBUG
	if (debug > 1)
		(void)printf( "sspit: len %d str `%s'\n", len, s);
#endif
	spit(len);
	while (*s)
		spit(*s++);
}

int 
yywrap()
{
	/* Always generate EOF */
	return (1);
}
