#	from: @(#)Makefile	8.2 (Berkeley) 12/15/93
#	$OpenBSD: Makefile,v 1.5 2001/05/25 10:23:04 hin Exp $
#	$NetBSD: Makefile,v 1.6 1996/02/24 01:15:15 jtk Exp $

LIB=	telnet
SRCS=	auth.c encrypt.c genget.c getent.c misc.c

CFLAGS+= -DHAS_CGETENT

.include <bsd.own.mk> # for KERBEROS

.if (${KERBEROS:L} == "yes")
CFLAGS+= -DKRB4 -DAUTHENTICATION -DENCRYPTION -DDES_ENCRYPTION
SRCS+= kerberos.c enc_des.c
.endif

.if (${KERBEROS5:L} == "yes")
CFLAGS+= -DKRB5 -DAUTHENTICATION -DENCRYPTION -DDES_ENCRYPTION
SRCS+= kerberos5.c
.endif

.include <bsd.lib.mk>
