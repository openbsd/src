/*	$NetBSD: qvss_compat.c,v 1.8 1997/05/25 10:53:33 jonathan Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)fb.c	8.1 (Berkeley) 6/10/93
 */

/* 
 *  devGraphics.c --
 *
 *     	This file contains machine-dependent routines for the graphics device.
 *
 *	Copyright (C) 1989 Digital Equipment Corporation.
 *	Permission to use, copy, modify, and distribute this software and
 *	its documentation for any purpose and without fee is hereby granted,
 *	provided that the above copyright notice appears in all copies.  
 *	Digital Equipment Corporation makes no representations about the
 *	suitability of this software for any purpose.  It is provided "as is"
 *	without express or implied warranty.
 *
 * from: Header: /sprite/src/kernel/dev/ds3100.md/RCS/devGraphics.c,
 *	v 9.2 90/02/13 22:16:24 shirriff Exp  SPRITE (DECWRL)";
 */

/*
 * This file has all the routines common to the various frame buffer drivers
 * including a generic ioctl routine. The pmax_fb structure is passed into the
 * routines and has device specifics stored in it.
 * The LK201 keycode mapping routine is also here along with initialization
 * functions for the keyboard and mouse.
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/mman.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <miscfs/specfs/specdev.h>

#include <machine/pmioctl.h>

#include <machine/fbio.h>
#include <machine/fbvar.h>
#include <pmax/dev/fbreg.h>		/* XXX should be renamed fbvar.h */
#include <pmax/dev/lk201.h>

/*#include <pmax/stand/dec_prom.h>*/

#include <pmax/pmax/cons.h>
#include <pmax/pmax/pmaxtype.h>

#include "dc_ds.h"
#include "dc_ioasic.h"
#include "scc.h"
#include "dtop.h"


/*
 * Forward / extern references.
 */

#include <pmax/dev/qvssvar.h>			/* our own externs */

struct termios; struct dcregs;
#include <pmax/dev/dtopvar.h>			/* dtop console I/O decls */
#include <pmax/tc/sccvar.h>			/* ioasic z8530 I/O decls */
#include <pmax/dev/dcvar.h>			/* DZ-11 chip console I/O */

extern int pmax_boardtype;

/*
 * Prototypes of local functions
 */
extern void pmEventQueueInit __P((pmEventQueue *qe));
void	genKbdEvent __P((int ch));
void	genMouseEvent __P((MouseReport *newRepPtr));
void	genMouseButtons __P((MouseReport *newRepPtr));
void	genConfigMouse __P((void));
void	genDeconfigMouse __P((void));
void	mouseInput __P((int cc));


#if NSCC > 0
extern void (*sccDivertXInput) __P((int cc));
extern void (*sccMouseEvent) __P((int));
extern void (*sccMouseButtons) __P((int));
#endif

extern struct fbinfo *firstfi;


/*
 * Initialize the old-style pmax framebuffer structure from a new-style
 * rcons structure. Can die when all the old style pmax_fb structures
 * are gone. Note that the QVSS/pm mapped event buffer includes the
 * fbu field initialized below.
 */
void
init_pmaxfbu(fi)
	struct fbinfo *fi;
{
	
	int tty_rows, tty_cols; /* rows, cols for glass-tty mode */
	register struct fbuaccess *fbu = NULL;

	if (fi == NULL || fi->fi_fbu == NULL)
		panic("init_pmaxfb: given null pointer to framebuffer");

	/* XXX don't rely on there being a pmax_fb struct */
	fbu = fi->fi_fbu;


	/* fb dimensions */
	fbu->scrInfo.max_x = fi->fi_type.fb_width;
	fbu->scrInfo.max_y = fi->fi_type.fb_height;
	fbu->scrInfo.max_cur_x = fbu->scrInfo.max_x - 1;
	fbu->scrInfo.max_cur_y = fbu->scrInfo.max_y - 1;

	/* these have the same  initial value on qvss-style framebuffers */
	fbu->scrInfo.version = 11;
	fbu->scrInfo.mthreshold = 4;
	fbu->scrInfo.mscale = 2;

	/* this is not always right (pm on kn01) but it's a common case */
	fbu->scrInfo.min_cur_x = 0;
	fbu->scrInfo.min_cur_y = 0;

	/*
	 * Compute glass-tty dimensions. These don't belong here
	 * anymore, but the Ultrix and 4.3+ bsd drivers put them
	 * in the event structure mapped into user address space.
	 */

	tty_cols = 80;

	/* A guess, but correct for 1024x864, 1024x768 and 1280x1024 */
	tty_rows = (fi->fi_type.fb_height / 15) - 1;

	if (tty_rows != fbu->scrInfo.max_row ||
	    tty_cols != fbu->scrInfo.max_col)
		printf("framebuffer init: size mismatch: given %dx%d, compute %dx%d\n",
		       fbu->scrInfo.max_row, fbu->scrInfo.max_col,
		       tty_rows, tty_cols);

	pmEventQueueInit(&fi->fi_fbu->scrInfo.qe);
}


/*
 * Initialize the qvss-style  ringbuffer of mouse button/move
 * events to be empty. Called both when initializing the
 * console softc and on each new open of that device.
 */
void
pmEventQueueInit(qe)
	pmEventQueue *qe;
{
	qe->timestamp_ms = TO_MS(time);
	qe->eSize = PM_MAXEVQ;
	qe->eHead = qe->eTail = 0;
	qe->tcSize = MOTION_BUFFER_SIZE;
	qe->tcNext = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * fbKbdEvent --
 *
 *	Process a received character.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Events added to the queue.
 *
 *----------------------------------------------------------------------
 */
void
fbKbdEvent(ch, fi)
	int ch;
	register struct fbinfo *fi;
{
	register pmEvent *eventPtr;
	int i;
	register struct fbuaccess *fbu = NULL;

	if (!fi->fi_open)
		return;

	fbu = fi->fi_fbu;

	/*
	 * See if there is room in the queue.
	 */
	i = PM_EVROUND(fbu->scrInfo.qe.eTail + 1);
	if (i == fbu->scrInfo.qe.eHead)
		return;

	/*
	 * Add the event to the queue.
	 */
	eventPtr = &fbu->events[fbu->scrInfo.qe.eTail];
	eventPtr->type = BUTTON_RAW_TYPE;
	eventPtr->device = KEYBOARD_DEVICE;
	eventPtr->x = fbu->scrInfo.mouse.x;
	eventPtr->y = fbu->scrInfo.mouse.y;
	eventPtr->time = TO_MS(time);
	eventPtr->key = ch;
	fbu->scrInfo.qe.eTail = i;
	selwakeup(&fi->fi_selp);
}

/*
 *----------------------------------------------------------------------
 *
 * fbMouseEvent --
 *
 *	Process a mouse event.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	An event is added to the event queue.
 *
 *----------------------------------------------------------------------
 */
void
fbMouseEvent(newRepPtr, fi) 
	register MouseReport *newRepPtr;
	register struct fbinfo *fi;
{
	unsigned milliSec;
	int i;
	pmEvent *eventPtr;
	register struct fbuaccess *fbu = NULL;

	if (!fi->fi_open)
		return;

	fbu = fi->fi_fbu;

	milliSec = TO_MS(time);

	/*
	 * Check to see if we have to accelerate the mouse
	 */
	if (fbu->scrInfo.mscale >= 0) {
		if (newRepPtr->dx >= fbu->scrInfo.mthreshold) {
			newRepPtr->dx +=
				(newRepPtr->dx - fbu->scrInfo.mthreshold) *
				fbu->scrInfo.mscale;
		}
		if (newRepPtr->dy >= fbu->scrInfo.mthreshold) {
			newRepPtr->dy +=
				(newRepPtr->dy - fbu->scrInfo.mthreshold) *
				fbu->scrInfo.mscale;
		}
	}

	/*
	 * Update mouse position
	 */
	if (newRepPtr->state & MOUSE_X_SIGN) {
		fbu->scrInfo.mouse.x += newRepPtr->dx;
		if (fbu->scrInfo.mouse.x > fbu->scrInfo.max_cur_x)
			fbu->scrInfo.mouse.x = fbu->scrInfo.max_cur_x;
	} else {
		fbu->scrInfo.mouse.x -= newRepPtr->dx;
		if (fbu->scrInfo.mouse.x < fbu->scrInfo.min_cur_x)
			fbu->scrInfo.mouse.x = fbu->scrInfo.min_cur_x;
	}
	if (newRepPtr->state & MOUSE_Y_SIGN) {
		fbu->scrInfo.mouse.y -= newRepPtr->dy;
		if (fbu->scrInfo.mouse.y < fbu->scrInfo.min_cur_y)
			fbu->scrInfo.mouse.y = fbu->scrInfo.min_cur_y;
	} else {
		fbu->scrInfo.mouse.y += newRepPtr->dy;
		if (fbu->scrInfo.mouse.y > fbu->scrInfo.max_cur_y)
			fbu->scrInfo.mouse.y = fbu->scrInfo.max_cur_y;
	}

	/*
	 * Move the hardware cursor.
	 */
	(*fi->fi_driver->fbd_poscursor)
		(fi, fbu->scrInfo.mouse.x, fbu->scrInfo.mouse.y);

	/*
	 * Store the motion event in the motion buffer.
	 */
	fbu->tcs[fbu->scrInfo.qe.tcNext].time = milliSec;
	fbu->tcs[fbu->scrInfo.qe.tcNext].x = fbu->scrInfo.mouse.x;
	fbu->tcs[fbu->scrInfo.qe.tcNext].y = fbu->scrInfo.mouse.y;
	if (++fbu->scrInfo.qe.tcNext >= MOTION_BUFFER_SIZE)
		fbu->scrInfo.qe.tcNext = 0;
	if (fbu->scrInfo.mouse.y < fbu->scrInfo.mbox.bottom &&
	    fbu->scrInfo.mouse.y >=  fbu->scrInfo.mbox.top &&
	    fbu->scrInfo.mouse.x < fbu->scrInfo.mbox.right &&
	    fbu->scrInfo.mouse.x >=  fbu->scrInfo.mbox.left)
		return;

	fbu->scrInfo.mbox.bottom = 0;
	if (PM_EVROUND(fbu->scrInfo.qe.eTail + 1) == fbu->scrInfo.qe.eHead)
		return;

	i = PM_EVROUND(fbu->scrInfo.qe.eTail - 1);
	if ((fbu->scrInfo.qe.eTail != fbu->scrInfo.qe.eHead) && 
	    (i != fbu->scrInfo.qe.eHead)) {
		pmEvent *eventPtr;

		eventPtr = &fbu->events[i];
		if (eventPtr->type == MOTION_TYPE) {
			eventPtr->x = fbu->scrInfo.mouse.x;
			eventPtr->y = fbu->scrInfo.mouse.y;
			eventPtr->time = milliSec;
			eventPtr->device = MOUSE_DEVICE;
			return;
		}
	}
	/*
	 * Put event into queue and wakeup any waiters.
	 */
	eventPtr = &fbu->events[fbu->scrInfo.qe.eTail];
	eventPtr->type = MOTION_TYPE;
	eventPtr->time = milliSec;
	eventPtr->x = fbu->scrInfo.mouse.x;
	eventPtr->y = fbu->scrInfo.mouse.y;
	eventPtr->device = MOUSE_DEVICE;
	fbu->scrInfo.qe.eTail = PM_EVROUND(fbu->scrInfo.qe.eTail + 1);
	selwakeup(&fi->fi_selp);
}

/*
 *----------------------------------------------------------------------
 *
 * fbMouseButtons --
 *
 *	Process mouse buttons.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
void
fbMouseButtons(newRepPtr, fi)
	MouseReport *newRepPtr;
	register struct fbinfo *fi;
{
	static char temp, oldSwitch, newSwitch;
	int i, j;
	pmEvent *eventPtr;
	static MouseReport lastRep;
	register struct fbuaccess *fbu = NULL;

	if (!fi->fi_open)
		return;

	fbu = fi->fi_fbu;

	newSwitch = newRepPtr->state & 0x07;
	oldSwitch = lastRep.state & 0x07;

	temp = oldSwitch ^ newSwitch;
	if (temp == 0)
		return;
	for (j = 1; j < 8; j <<= 1) {
		if ((j & temp) == 0)
			continue;

		/*
		 * Check for room in the queue
		 */
		i = PM_EVROUND(fbu->scrInfo.qe.eTail+1);
		if (i == fbu->scrInfo.qe.eHead)
			return;

		/*
		 * Put event into queue.
		 */
		eventPtr = &fbu->events[fbu->scrInfo.qe.eTail];

		switch (j) {
		case RIGHT_BUTTON:
			eventPtr->key = EVENT_RIGHT_BUTTON;
			break;

		case MIDDLE_BUTTON:
			eventPtr->key = EVENT_MIDDLE_BUTTON;
			break;

		case LEFT_BUTTON:
			eventPtr->key = EVENT_LEFT_BUTTON;
		}
		if (newSwitch & j)
			eventPtr->type = BUTTON_DOWN_TYPE;
		else
			eventPtr->type = BUTTON_UP_TYPE;
		eventPtr->device = MOUSE_DEVICE;

		eventPtr->time = TO_MS(time);
		eventPtr->x = fbu->scrInfo.mouse.x;
		eventPtr->y = fbu->scrInfo.mouse.y;
		fbu->scrInfo.qe.eTail = i;
	}
	selwakeup(&fi->fi_selp);

	lastRep = *newRepPtr;
	fbu->scrInfo.mswitches = newSwitch;
}

/*
 * Use vm_mmap() to map the frame buffer and shared data into the user's
 * address space.
 * Return errno if there was an error.
 */
int
fbmmap_fb(fi, dev, data, p)
	struct fbinfo *fi;
	dev_t dev;
	caddr_t data;
	struct proc *p;
{
	int error;
	vm_offset_t addr;
	vm_size_t len;
	struct vnode vn;
	struct specinfo si;
	struct fbuaccess *fbp;
	register struct fbuaccess *fbu = fi->fi_fbu;

	len = mips_round_page(((vm_offset_t)fbu & PGOFSET) +
			      sizeof(struct fbuaccess)) +
		mips_round_page(fi->fi_type.fb_size);
	addr = (vm_offset_t)0x20000000;		/* XXX */
	vn.v_type = VCHR;			/* XXX */
	vn.v_specinfo = &si;			/* XXX */
	vn.v_rdev = dev;			/* XXX */
	/*
	 * Map the all the data the user needs access to into
	 * user space.
	 */
	error = vm_mmap(&p->p_vmspace->vm_map, &addr, len,
		VM_PROT_ALL, VM_PROT_ALL, MAP_SHARED, (caddr_t)&vn,
		(vm_offset_t)0);
	if (error)
		return (error);
	fbp = (struct fbuaccess *)(addr + ((vm_offset_t)fbu & PGOFSET));
	*(PM_Info **)data = &fbp->scrInfo;
	fbu->scrInfo.qe.events = fbp->events;
	fbu->scrInfo.qe.tcs = fbp->tcs;
	fbu->scrInfo.planemask = (char *)0;
	/*
	 * Map the frame buffer into the user's address space.
	 */
	fbu->scrInfo.bitmap = (char *)mips_round_page(fbp + 1);
	return (0);
}

/*
 * Generic functions for keyboard and mouse input.
 * Just use the "generic" qvss/pm-compatible functions  above, but pass them
 * the soft state for the first framebuffer found on this system.
 * We don't support more  than one mouse, even for multiple
 * framebuffers, so this should be adequate.
 * It also relieves each fb driver from having to provide its own
 * version of these functions.
 *
 * TODO: change the callers of these to pass a pointer to the struct fbinfo,
 * thus finessing the problem.
 */

void
genKbdEvent(ch)
	int ch;
{
	fbKbdEvent(ch, firstfi);
}

void
genMouseEvent(newRepPtr)
	MouseReport *newRepPtr;
{
	fbMouseEvent(newRepPtr, firstfi);
}

void
genMouseButtons(newRepPtr)
	MouseReport *newRepPtr;
{
	fbMouseButtons(newRepPtr, firstfi);
}

/*
 * Configure the mouse and keyboard based on machine type
 */
void
genConfigMouse()
{
	int s;

	s = spltty();
	switch (pmax_boardtype) {
#if NDC_IOASIC > 0
	case DS_3MAX:
		dcDivertXInput = genKbdEvent;
		dcMouseEvent = (void (*) __P((int)))genMouseEvent;
		dcMouseButtons = (void (*) __P((int)))genMouseButtons;
		break;
#endif /* NDC_IOASIC */

#if NDC_DS > 0
	case DS_PMAX:
		dcDivertXInput = genKbdEvent;
		dcMouseEvent = (void (*) __P((int)))genMouseEvent;
		dcMouseButtons = (void (*) __P((int)))genMouseButtons;
		break;
#endif /* NDC_DS */

#if NSCC > 0
	case DS_3MIN:
	case DS_3MAXPLUS:
		sccDivertXInput = (void (*) __P((int)))genKbdEvent;
		sccMouseEvent = (void (*) __P((int)))genMouseEvent;
		sccMouseButtons = (void (*) __P((int)))genMouseButtons;
		break;
#endif
#if NDTOP > 0
	case DS_MAXINE:
		dtopDivertXInput = genKbdEvent;
		dtopMouseEvent = genMouseEvent;
		dtopMouseButtons = genMouseButtons;
		break;
#endif
	default:
		printf("Can't configure mouse/keyboard\n");
	};
	splx(s);
}

/*
 * and deconfigure them
 */
void
genDeconfigMouse()
{
	int s;

	s = spltty();
	switch (pmax_boardtype) {
#if NDC_IOASIC > 0
	case DS_3MAX:

		dcDivertXInput = (void (*) __P((int)) )0;
		dcMouseEvent = (void (*) __P((int)) )0;
		dcMouseButtons = (void (*) __P((int)) )0;
		break;
#endif  /* NDC_IOASIC */

#if NDC_DS > 0
	case DS_PMAX:
		dcDivertXInput = (void (*) __P((int)) )0;
		dcMouseEvent = (void (*) __P((int)) )0;
		dcMouseButtons =  (void (*) __P((int)) )0;
		break;
#endif /* NDC_DS */

#if NSCC > 0
	case DS_3MIN:
	case DS_3MAXPLUS:
		sccDivertXInput = (void (*) __P((int)))0;
		sccMouseEvent = (void (*) __P((int)))0;
		sccMouseButtons = (void (*) __P((int)))0;
		break;
#endif

#if NDTOP > 0
	case DS_MAXINE:
		dtopDivertXInput = (void (*) __P((int)) )0;
		dtopMouseEvent = (void (*) __P((MouseReport *)) )0;
		dtopMouseButtons = (void (*) __P((MouseReport *)) )0;
		break;
#endif
	default:
		printf("Can't deconfigure mouse/keyboard\n");
	};
}


/**
 ** And a mouse-report handler for redirected mouse input.
 ** Could arguably be in its own source file, but it's only
 ** used when the kernel is performing  mouse tracking.
 **/

/*
 * Mouse-event parser.  Called as an upcall with each character
 * read from a serial port. Accumulates complete mouse-event
 *  reports and passes them up to framebuffer layer.
 * Mouse events are reported as a 3-byte sequence:
 * header+button state, delta-x, delta-y
 */
void
mouseInput(cc)
	int cc;
{
	register MouseReport *mrp;
	static MouseReport currentRep;

	mrp = &currentRep;
	mrp->byteCount++;
	if (cc & MOUSE_START_FRAME) {
		/*
		 * The first mouse report byte (button state).
		 */
		mrp->state = cc;
		if (mrp->byteCount > 1)
			mrp->byteCount = 1;
	} else if (mrp->byteCount == 2) {
		/*
		 * The second mouse report byte (delta x).
		 */
		mrp->dx = cc;
	} else if (mrp->byteCount == 3) {
		/*
		 * The final mouse report byte (delta y).
		 */
		mrp->dy = cc;
		mrp->byteCount = 0;
		if (mrp->dx != 0 || mrp->dy != 0) {
			/*
			 * If the mouse moved,
			 * post a motion event.
			 */
			(genMouseEvent)(mrp);
		}
		(genMouseButtons)(mrp);
	}
}
