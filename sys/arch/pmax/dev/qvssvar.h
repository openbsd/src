/*	$NetBSD: qvssvar.h,v 1.1 1996/05/19 00:48:43 jonathan Exp $	*/
/*
 * Copyright 1996 The Board of Trustees of The Leland Stanford
 * Junior University. All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  Stanford University
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 * This file contributed by Jonathan Stone.
 */

#ifdef _KERNEL

void	pmEventQueueInit __P((pmEventQueue *qe));

void	genKbdEvent __P((int ch));
void	genMouseEvent __P((MouseReport *newRepPtr));
void	genMouseButtons __P((MouseReport *newRepPtr));
void	genConfigMouse __P((void));
void	genDeconfigMouse __P((void));
void	mouseInput __P((int cc));

#endif	/* _KERNEL */
