/*	$OpenBSD: i386.c,v 1.10 2009/10/27 23:59:38 deraadt Exp $	*/
/*	$NetBSD: i386.c,v 1.5 1995/04/19 07:16:04 cgd Exp $	*/

/*-
 * Copyright (c) 1996 SigmaSoft, Th. Lockert
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

#include "gprof.h"

#define	iscall(off)	((*(u_char *)&textspace[off]) == 0xE8)

void
findcall(nltype *parentp, unsigned long p_lowpc, unsigned long p_highpc)
{
	unsigned long pc;
	long len;
	nltype *childp;
	unsigned long destpc;
	int off;

	if (textspace == 0)
		return;
	if (p_lowpc < s_lowpc)
		p_lowpc = s_lowpc;
	if (p_highpc > s_highpc)
		p_highpc = s_highpc;
#	ifdef DEBUG
		if ( debug & CALLDEBUG ) {
			printf( "[findcall] %s: 0x%x to 0x%x\n" ,
				parentp -> name , p_lowpc , p_highpc );
		}
#	endif /* DEBUG */
	for (pc = p_lowpc; pc < p_highpc; pc += len) {
		off = pc - s_lowpc;
		len = 1;
		if (iscall(off)) {
			destpc = *(unsigned long *)&textspace[off + 1] + off + 5;
#			ifdef DEBUG
				if ( debug & CALLDEBUG ) {
					printf( "[findcall]\t0x%x:calls" , pc - textspace );
					printf( "\tdestpc 0x%x" , destpc );
				}
#			endif /* DEBUG */
			if (destpc >= s_lowpc && destpc <= s_highpc) {
				childp = nllookup(destpc);
#				ifdef DEBUG
					if ( debug & CALLDEBUG ) {
						printf( " childp->name %s" , childp -> name );
						printf( " childp->value 0x%x\n" ,
							childp -> value );
					}
#				endif /* DEBUG */
				if (childp != NULL && childp->value == destpc) {
					addarc(parentp, childp, 0L);
					len += 4;
					continue;
				}
			}
#			ifdef DEBUG
				if ( debug & CALLDEBUG ) {
					printf( "\tbut it's a botch\n" );
				}
#			endif /* DEBUG */
		}
	}
}
