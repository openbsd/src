#	$OpenBSD: Makefile,v 1.9 2000/02/25 17:23:36 niklas Exp $
#	$EOM: Makefile,v 1.8 2000/02/20 19:58:44 niklas Exp $

#
# Copyright (c) 1999 Niels Provos.  All rights reserved.
# Copyright (c) 1999 Niklas Hallqvist.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#	This product includes software developed by Ericsson Radio Systems.
# 4. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
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
SRCS=		libcrypto.c log.c rsakeygen.c sysdep.c util.c
TOPSRC=		${.CURDIR}/../..
TOPOBJ!=	cd ${TOPSRC}; printf "all:\n\t@pwd\n" |${MAKE} -f-
OS!=		awk '/^OS=/ { print $$2 }' ${.CURDIR}/../../Makefile
.PATH:		${TOPSRC} ${TOPSRC}/sysdep/${OS} ${TOPOBJ}
CFLAGS+=	-I${TOPSRC} -I${TOPSRC}/sysdep/${OS} -I${TOPOBJ} -Wall \
		-DUSE_DEBUG
NOMAN=
DEBUG=		-g

.include "${TOPSRC}/sysdep/${OS}/Makefile.sysdep"

.ifdef HAVE_DLOPEN
CFLAGS+=	-DHAVE_DLOPEN
SRCS+=		dyn.c
.endif

.ifdef USE_LIBCRYPTO
CFLAGS+=	-DUSE_LIBCRYPTO
LDADD+=		-lcrypto
DPADD+=		${LIBCRYPTO}
.endif

.if !defined (HAVE_DLOPEN) && !defined (USE_LIBCRYPTO)
.BEGIN:
	@echo RSA cannot be used in this environmet, skipping...

PROG=
.endif

LDADD+=		${DESLIB}
DPADD+=		${DESLIBDEP}

.include <bsd.prog.mk>
