/*	$OpenBSD: map_scan.l,v 1.3 2008/06/26 05:42:06 ray Exp $	*/
/*	$NetBSD: map_scan.l 1.1 1998/12/28 14:01:17 hannken Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Juergen Hannken-Illjes.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

%{

#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>
#include <err.h>
#include "wsconsctl.h"
#include "y.tab.h"

#define yywrap()	1

void
map_scan_setinput(str)
	char *str;
{
	yy_scan_string(str);
}

%}

%%

[ \t\n]+ {
		/* ignore spaces and tabs */
	}

"=" {
		return('=');
	}

keycode {
		return(T_KEYCODE);
	}

keysym {
		return(T_KEYSYM);
	}

[a-zA-Z][a-zA-Z0-9_]* {
		int i;

		i = name2ksym(yytext);
		if (i == -1)
			errx(1, "%s: not a keysym", yytext);
		yylval.kval = i;
		if (KS_GROUP(i) == KS_GROUP_Command ||
		    i == KS_Cmd || i == KS_Cmd1 || i == KS_Cmd2)
			return(T_KEYSYM_CMD_VAR);
		else
			return(T_KEYSYM_VAR);
	}

[0-9]+ {
		yylval.ival = atoi(yytext);
		return(T_NUMBER);
	}

. {
		if (yytext[0] >= ' ' && yytext[0] <= '~')
			errx(1, "%c: illegal character in input", yytext[0]);
		else
			errx(1, "%03o: illegal character in input", yytext[0] & 255);

		/* To quiet the compiler */
		if (0)
			unput(0);
	}
