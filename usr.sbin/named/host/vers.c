/*	$OpenBSD: vers.c,v 1.2 1997/03/12 10:41:57 downsj Exp $	*/

#ifndef lint
static char Version[] = "@(#)vers.c	e07@nikhef.nl (Eric Wassenaar) 961113";
#endif

char *version = "961113";

#if defined(apollo)
int h_errno = 0;
#endif
