/*	$OpenBSD: i386.c,v 1.3 1996/10/02 02:59:50 tholo Exp $	*/
/*	$NetBSD: i386.c,v 1.5 1995/04/19 07:16:04 cgd Exp $	*/

#ifndef lint
static char rcsid[] = "$OpenBSD: i386.c,v 1.3 1996/10/02 02:59:50 tholo Exp $";
#endif /* not lint */

#include "gprof.h"

#define	iscall(pc)	((*pc) == 0xE8)

/*
 * gprof -c isn't currently supported...
 */
findcall( parentp , p_lowpc , p_highpc )
    nltype		*parentp;
    unsigned long	p_lowpc;
    unsigned long	p_highpc;
{
	unsigned char *pc;
	long len;
	nltype *childp;
	unsigned long destpc;

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
#	endif DEBUG
	for (pc = textspace + p_lowpc - N_TXTADDR(xbuf) ; pc < textspace + p_highpc - N_TXTADDR(xbuf) ; pc += len) {
		len = 1;
		if (iscall(pc)) {
			destpc = *(unsigned long *)(pc + 1) + (pc - textspace + N_TXTADDR(xbuf)) + 5;
#			ifdef DEBUG
				if ( debug & CALLDEBUG ) {
					printf( "[findcall]\t0x%x:calls" , pc - textspace );
					printf( "\tdestpc 0x%x" , destpc );
				}
#			endif DEBUG
			if (destpc >= s_lowpc && destpc <= s_highpc) {
				childp = nllookup(destpc);
#				ifdef DEBUG
					if ( debug & CALLDEBUG ) {
						printf( " childp->name %s" , childp -> name );
						printf( " childp->value 0x%x\n" ,
							childp -> value );
					}
#				endif DEBUG
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
#			endif DEBUG
		}
	}
}
