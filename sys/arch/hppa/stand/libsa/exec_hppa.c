/*	$OpenBSD: exec_hppa.c,v 1.2 1998/07/13 03:35:59 mickey Exp $	*/
/*	$NOWHERE: exec_hppa.c,v 2.2 1998/06/22 19:34:46 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
 *	This product includes software developed by Tobias Weingartner.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/reboot.h>
#include <dev/cons.h>
#include <stand/boot/bootarg.h>
#include <sys/disklabel.h>
#include "libsa.h"
#include <lib/libsa/exec.h>

#define round_to_size(x) (((int)(x) + sizeof(int) - 1) & ~(sizeof(int) - 1))

typedef void (*startfuncp) __P((int, int, caddr_t))
    __attribute__ ((noreturn));

void
machdep_exec(xp, howto, loadaddr)
	struct x_param *xp;
	int howto;
	void *loadaddr;
{
	size_t ac = BOOTARG_LEN;
	caddr_t av = (caddr_t)BOOTARG_OFF;
	makebootargs(av, &ac);

	fcacheall();

	/* stack and the gung is ok at this point, so, no need for asm setup */
	(*(startfuncp)(loadaddr + xp->xp_entry)) (BOOTARG_APIVER, ac, av);
	/* not reached */
}
