/*	$NetBSD: db_disasm.c,v 1.2 1995/11/30 00:59:34 jtc Exp $ */
/*
 * Copyright (c) 1995 Ludd, University of Lule}, Sweden.
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
 *      This product includes software developed at Ludd, University of 
 *      Lule}, Sweden and its contributors.
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


#include <sys/param.h>
#include <sys/proc.h>
#include <sys/reboot.h>

#include <ddb/db_variables.h>

#include <machine/db_machdep.h>



struct vax_insn {
	char	*insn;
	int 	nargs;
} instr[] = {
	"halt",	0,
	"nop",	0,
	"rei",	0,
	"bpt",	0,
	"ret",	0,
	"rsb",	0,
	"ldpctx",	0,
	"svpctx",	0,
	"cvtps",	4,
	"cvtsp",	4,
	"index",	6,
	"crc",	4,
	"prober",	3,
	"probew",	3,
	"insque",	2,
	"remque",	2,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
	"",	-1,
};


/*
 * Disassemble instruction at 'loc'.  'altfmt' specifies an
 * (optional) alternate format.  Return address of start of
 * next instruction.
 */
db_addr_t
db_disasm(loc, altfmt)
        db_addr_t       loc;
        boolean_t       altfmt;
{
	char *i_pl;
	int inr, i;

	i_pl = (char *)loc;
	inr = *i_pl;

	if (instr[*i_pl].nargs < 0) {
		printf("Ok{nd instruktion: %2x",*i_pl&0xff);
		i_pl++;
	} else {
		printf("\t%s\t",instr[inr].insn);
		i_pl++;
		for (i=0;i<instr[inr].nargs;i++) {
			i_pl = (char *)argprint(i_pl);
			if (i<instr[inr].nargs-1)
				printf(",");
		}
	}



        return (int)i_pl;
}

argprint(plats)
	char *plats;
{
	switch (*plats&0xf0) {
	case 0x00:
	case 0x10:
	case 0x20:
	case 0x30:
		printf("$%x",*plats++);
		break;
		
	case 0xe0:
		if (*plats++&15 == 15) {
			printf("%8x",*(unsigned *)plats + plats);
			plats += 4;
		} else {
			printf("Oinpl. s{tt.");
		}
		break;
	default:
		printf("Oinpl. s{tt.");
	}
	return (int)plats;
}
