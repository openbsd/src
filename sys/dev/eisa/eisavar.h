/*	$NetBSD: eisavar.h,v 1.1 1995/04/17 12:08:23 cgd Exp $	*/

/*
 * Copyright (c) 1995 Christopher G. Demetriou
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
 *      This product includes software developed by Christopher G. Demetriou
 *      for the NetBSD Project.
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
 * XXX
 * XXX EISA AUTOCONFIG SHOULD BE SEPERATED FROM ISA AUTOCONFIG!!!
 * XXX
 */

/*
 * pull in the ISA definitions
 */
#include <dev/isa/isavar.h>

/*
 * and bend them to our twisted ways:
 * map the functions, etc. that are used
 */

#define eisa_attach_args	isa_attach_args			/* XXX */
#define eisadev			isadev				/* XXX */
#define	eisa_intr_establish	isa_intr_establish		/* XXX */
#define	eisa_intr_disestablish	isa_intr_disestablish		/* XXX */

#define	EISA_IPL_NONE	ISA_IPL_NONE				/* XXX */
#define	EISA_IPL_BIO	ISA_IPL_BIO				/* XXX */
#define	EISA_IPL_NET	ISA_IPL_NET				/* XXX */
#define	EISA_IPL_TTY	ISA_IPL_TTY				/* XXX */
#define	EISA_IPL_CLOCK	ISA_IPL_CLOCK				/* XXX */

#define	EISA_IST_PULSE	ISA_IST_PULSE				/* XXX */
#define	EISA_IST_EDGE	ISA_IST_EDGE				/* XXX */
#define	EISA_IST_LEVEL	ISA_IST_LEVEL				/* XXX */
