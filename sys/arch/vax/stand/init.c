/* $NetBSD: init.c,v 1.3 1995/09/16 13:34:21 ragge Exp $ */
/*
 * Copyright (c) 1995 Ludd, University of Lule}, Sweden. All rights reserved.
 * 
 * This code is derived from software contributed to Ludd by Bertram Barth.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer. 2.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution. 3. All advertising
 * materials mentioning features or use of this software must display the
 * following acknowledgement: This product includes software developed at
 * Ludd, University of Lule}, Sweden and its contributors. 4. The name of the
 * author may not be used to endorse or promote products derived from this
 * software without specific prior written permission
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* All bugs are subject to removal without further notice */



#include "lib/libsa/stand.h"

#include "../include/mtpr.h"	/* mfpr(), mtpr() */
#include "../include/sid.h"	/* cpu_type, cpu_number */

#define NRSP 0			/* Kludge, must be done before udareg.h */
#define NCMD 0			/* includes */

#include "../uba/udareg.h"	/* struct udadevice */

#include "data.h"		/* bootregs[], rpb, bqo */

struct rpb     *rpb;
struct bqo     *bqo;

int	is_750 = 0, is_mvax = 0;

/*
 * initdata() sets up data gotten from start routines, mostly for uVAX.
 */
int
initdata()
{
	int i, *tmp;

	cpu_type = mfpr(PR_SID);
	cpunumber = (mfpr(PR_SID) >> 24) & 0xFF;

	switch (cpunumber) {

	case VAX_78032:
	case VAX_650:
	{
		int             cpu_sie;	/* sid-extension */

		is_mvax = 1;
		cpu_sie = *((int *) 0x20040004) >> 24;
		cpu_type |= cpu_sie;

		break;
	}
	case VAX_750:
		is_750 = 1;
		break;
	}
	return 0;
}
