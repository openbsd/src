#	$OpenBSD: Makefile,v 1.1 2005/04/08 17:12:50 cloder Exp $
#	$EOM: Makefile,v 1.10 2000/03/28 21:23:24 ho Exp $

#
# Copyright (c) 1999 Niels Provos.  All rights reserved.
# Copyright (c) 1999, 2001 Niklas Hallqvist.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

#
# This code was written under funding by Ericsson Radio Systems.
#

# RSA Key Generation

PROG=		rsakeygen
SRCS=		libcrypto.c log.c rsakeygen.c sysdep.c util.c conf.c
TOPSRC=		${.CURDIR}/../../../../sbin/isakmpd
TOPOBJ!=	cd ${TOPSRC}; printf "all:\n\t@pwd\n" |${MAKE} -f-
OS!=		awk '/^OS=/ { print $$2 }' ${.CURDIR}/../../Makefile
FEATURES!=	awk '/^FEATURES=/ { print $$0 }' ${.CURDIR}/../../Makefile | sed 's/FEATURES=.//'
.PATH:		${TOPSRC} ${TOPSRC}/sysdep/openbsd ${TOPOBJ}
CFLAGS+=	-I${TOPSRC} -I${TOPSRC}/sysdep/openbsd -I${TOPOBJ} -Wall \
		-DUSE_DEBUG
NOMAN=
DEBUG=		-g

.if ${FEATURES:Mgmp} == "gmp"
CFLAGS+=	-DMP_FLAVOUR=MP_FLAVOUR_GMP
LDADD+=		-lgmp
DPADD+=		${LIBGMP}
.else
CFLAGS+=	-DMP_FLAVOUR=MP_FLAVOUR_OPENSSL
.endif

.include "${TOPSRC}/sysdep/openbsd/Makefile.sysdep"

.ifdef HAVE_DLOPEN
CFLAGS+=	-DHAVE_DLOPEN
SRCS+=		dyn.c
.endif

CFLAGS+=	-DUSE_LIBCRYPTO
LDADD+=		-lcrypto
DPADD+=		${LIBCRYPTO}

LDADD+=		${DESLIB}
DPADD+=		${DESLIBDEP}

.include <bsd.regress.mk>
