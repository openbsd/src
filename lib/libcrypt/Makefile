#	$NetBSD: Makefile,v 1.6 1995/02/19 12:18:56 cgd Exp $
#
# This directory contains an implementation of crypt(3) and associated
# routines.  They have been modified so that decryption is no longer possible,
# and thus are exportable.  They were taken from the 4.4BSD-Lite foreign
# distribution.

LIB=	crypt

SRCS=	crypt.c

MAN=	crypt.3
MLINKS= crypt.3 encrypt.3 crypt.3 setkey.3

.include <bsd.lib.mk>
