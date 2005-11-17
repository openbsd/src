/*	$OpenBSD: llround.c,v 1.1 2005/11/17 20:07:40 otto Exp $	*/
/* $NetBSD: llround.c,v 1.2 2004/10/13 15:18:32 drochner Exp $ */

/*
 * Written by Matthias Drochner <drochner@NetBSD.org>.
 * Public domain.
 */

#define LROUNDNAME llround
#define RESTYPE long long int
#define RESTYPE_MIN LLONG_MIN
#define RESTYPE_MAX LLONG_MAX

#include "lround.c"
