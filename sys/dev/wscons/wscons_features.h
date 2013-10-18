/* $OpenBSD: wscons_features.h,v 1.1 2013/10/18 13:54:08 miod Exp $ */
/* public domain */

/*
 * This file contains the logic used to enable several optional features
 * of the wscons framework:
 *
 * HAVE_WSMOUSED_SUPPORT
 *	defined to enable support for wsmoused(8)
 * HAVE_BURNER_SUPPORT
 *	defined to enable screen blanking functionnality, controlled by
 *	wsconsctl(8)
 * HAVE_SCROLLBACK_SUPPORT
 *	defined to enable xterm-like shift-PgUp scrollback if the underlying
 *	wsdisplay supports this
 * HAVE_JUMP_SCROLL
 *	defined to enable jump scroll in the textmode emulation code
 */

#ifdef _KERNEL

#ifndef	SMALL_KERNEL
#define	HAVE_WSMOUSED_SUPPORT
#define	HAVE_BURNER_SUPPORT
#define	HAVE_SCROLLBACK_SUPPORT
#define	HAVE_JUMP_SCROLL
#endif

#endif
