#	from: @(#)Makefile	8.2 (Berkeley) 12/15/93
#	$Id: Makefile,v 1.1.1.1 1995/10/18 08:43:11 deraadt Exp $

LIB=	telnet
SRCS=	auth.c encrypt.c genget.c getent.c misc.c
CFLAGS+= -DHAS_CGETENT

# These are the sources that have encryption stuff in them.
CRYPT_SRC= auth.c enc-proto.h enc_des.c encrypt.c
CRYPT_SRC+= encrypt.h kerberos.c kerberos5.c krb4encpwd.c
CRYPT_SRC+= misc.c spx.c Makefile
NOCRYPT_DIR=${.CURDIR}/Nocrypt

.include <bsd.lib.mk>

nocrypt:
	@echo "Encryption code already removed."
