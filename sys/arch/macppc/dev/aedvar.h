/*	$OpenBSD: aedvar.h,v 1.1 2001/09/01 15:50:00 drahn Exp $	*/
/*	$NetBSD: aedvar.h,v 1.2 2000/03/23 06:40:33 thorpej Exp $	*/

/*
 * Copyright (C) 1994	Bradley A. Grantham
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
 *	This product includes software developed by Bradley A. Grantham.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef __NetBSD__
#include <sys/callout.h>
#endif /* __NetBSD__ */
#ifdef __OpenBSD__
#include <sys/timeout.h>
#endif /* __OpenBSD__ */
#include <machine/adbsys.h>

/* Event queue definitions */
#ifndef AED_MAX_EVENTS
#define AED_MAX_EVENTS 200	/* Maximum events to be kept in queue */  
				/* maybe should be higher for slower macs? */
#endif				/* AED_MAX_EVENTS */

struct aed_softc {
	struct  device  sc_dev;

#ifdef __NetBSD__
	struct callout sc_repeat_ch;
#endif /* __NetBSD__ */
#ifdef __OpenBSD__
	struct timeout sc_repeat_ch;
#endif /* __OpenBSD__ */

	/* ADB info */
	u_char		origaddr;	/* ADB device type (ADBADDR_AED) */
	u_char		adbaddr;	/* current ADB address */
	u_char		handler_id;	/* type of device */

	/* ADB event queue */
	adb_event_t	sc_evq[AED_MAX_EVENTS];	/* the queue */
	int		sc_evq_tail;	/* event queue tail pointer */
	int		sc_evq_len;	/* event queue length */

	/* Keyboard repeat state */
	int		sc_rptdelay;	/* ticks before auto-repeat */
	int		sc_rptinterval;	/* ticks between auto-repeat */
	int		sc_repeating;	/* key that is auto-repeating */
	adb_event_t	sc_rptevent;	/* event to auto-repeat */

	int		sc_buttons;	/* mouse button state */

	struct selinfo	sc_selinfo;	/* select() info */
	struct proc *	sc_ioproc;	/* process to wakeup */

	int		sc_open;	/* Are we queuing events? */
	int		sc_options;	/* config options */
};

/* Options */
#define AED_MSEMUL	0x1		/* emulate mouse buttons */

void	aed_input __P((adb_event_t *event));
int	aedopen __P((dev_t dev, int flag, int mode, struct proc *p));
int	aedclose __P((dev_t dev, int flag, int mode, struct proc *p));
int	aedread __P((dev_t dev, struct uio *uio, int flag));
int	aedwrite __P((dev_t dev, struct uio *uio, int flag));
int	aedioctl __P((dev_t , int , caddr_t , int , struct proc *));
int	aedpoll __P((dev_t dev, int events, struct proc *p));
