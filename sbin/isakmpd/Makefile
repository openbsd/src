#	$OpenBSD: Makefile,v 1.57 2004/08/23 11:16:49 ho Exp $
#	$EOM: Makefile,v 1.78 2000/10/15 21:33:42 niklas Exp $

#
# Copyright (c) 1998, 1999, 2000, 2001 Niklas Hallqvist.  All rights reserved.
# Copyright (c) 2000, 2001 Håkan Olsson.  All rights reserved.
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

#
# This makefile is a "pmake" one, i.e. the make variant commonly found in
# BSD derived systems, where it is indeed named "make".  Other systems
# may provide this make variant as "pmake" or maybe "bsdmake".
#

# openbsd means OpenBSD 2.5 or newer. freeswan is the name for Linux with
# FreeS/WAN integrated, freebsd/netbsd means FreeBSD/NetBSD with KAME IPsec.
OS=		openbsd
#OS=		netbsd
#OS=		freebsd
#OS=		freeswan
#OS=		bsdi

# Compile-time configuration of otherwise optional features
#FEATURES=	tripledes des blowfish cast aes
#FEATURES+=	policy x509 ec aggressive debug gmp
#FEATURES+=	rawkey isakmp_cfg dnssec privsep nat_traversal dpd
FEATURES=	tripledes des blowfish cast aes
FEATURES+=	policy x509 ec aggressive debug
FEATURES+=	rawkey isakmp_cfg privsep nat_traversal dpd

.PATH:		${.CURDIR}/sysdep/${OS}

PROG=		isakmpd
BINDIR?=	/sbin
LDSTATIC?=	-static
SRCS=		app.c attribute.c cert.c connection.c constants.c conf.c \
		cookie.c crypto.c dh.c doi.c exchange.c exchange_num.c \
		field.c gmp_util.c hash.c if.c ike_auth.c ike_main_mode.c \
		ike_phase_1.c ike_quick_mode.c init.c ipsec.c ipsec_fld.c \
		ipsec_num.c isakmpd.c isakmp_doi.c isakmp_fld.c isakmp_num.c \
		key.c libcrypto.c log.c message.c math_2n.c math_group.c \
		prf.c sa.c sysdep.c timer.c transport.c virtual.c udp.c \
		ui.c util.c

GENERATED=	exchange_num.h ipsec_fld.h ipsec_num.h isakmp_fld.h \
		isakmp_num.h
CLEANFILES=	exchange_num.c exchange_num.h ipsec_num.c ipsec_num.h \
		isakmp_num.c isakmp_num.h ipsec_fld.c ipsec_fld.h \
		isakmp_fld.c isakmp_fld.h
MAN=		isakmpd.8 isakmpd.conf.5 isakmpd.policy.5
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes \
		-Wmissing-declarations -DNEED_SYSDEP_APP \
		-I${.CURDIR} -I${.CURDIR}/sysdep/${OS} -I.
#CFLAGS+=	-Wsign-compare -Werror

# Different debugging & profiling suggestions

# Include symbolic debugging info
#DEBUG=		-g	# OpenBSD
#DEBUG_FLAGS=	-g	# FreeBSD
#CFLAGS+=	-g	# NetBSD and others
#STRIPFLAG=		# NETBSD

# Do execution time profiles
#CFLAGS+=	-pg

# If you have ElectricFence available, you can spot abuses of the heap.
# (/usr/ports/devel/ElectricFence)
#LDADD+=		-L/usr/local/lib -lefence
#DPADD+=		/usr/local/lib/libefence.a

# If you like to use Boehm's garbage collector (/usr/ports/devel/boehm-gc).
#LDADD+=		-L/usr/local/lib -lgc
#DPADD+=		/usr/local/lib/libgc.a
#CFLAGS+=		-DUSE_BOEHM_GC -DGC_DEBUG
# You can also use Boehm's garbage collector as a means to find leaks.
#  # setenv GC_FIND_LEAK

SUBDIR=		apps

.include "sysdep/${OS}/Makefile.sysdep"

.if ${FEATURES} != ""
FEATURES_UC!=	echo ${FEATURES} | tr '[:lower:]' '[:upper:]'
CFLAGS+=	${FEATURES_UC:S/^/-DUSE_/g}
.endif

.if make(install)
SUBDIR+=	samples
.endif

.if !make(install) && !defined(NO_REGRESS)
SUBDIR+=	regress
.endif

.for FEATURE in ${FEATURES}
.if exists(features/${FEATURE})
.include "features/${FEATURE}"
.endif
.endfor

.if ${FEATURES:Mgmp} == "gmp"
CFLAGS+=	-DMP_FLAVOUR=MP_FLAVOUR_GMP
LDADD+=		-lgmp
DPADD+=		${LIBGMP}
.else
CFLAGS+=	-DMP_FLAVOUR=MP_FLAVOUR_OPENSSL
.endif

.ifdef USE_KEYNOTE
USE_LIBCRYPTO=	yes
USE_LIBDES=	yes
LDADD+=		-lkeynote -lm
DPADD+=		${LIBKEYNOTE} ${LIBM}
CFLAGS+=	-DUSE_KEYNOTE
.endif

.ifdef USE_RAWKEY
USE_LIBCRYPTO=	yes
CFLAGS+=	-DUSE_RAWKEY
.endif

.ifdef USE_LIBCRYPTO
CFLAGS+=	-DUSE_LIBCRYPTO
LDADD+=		-lcrypto
DPADD+=		${LIBCRYPTO}
.endif

.ifdef USE_LIBDES
CFLAGS+=	-DUSE_LIBDES
LDADD+=		-ldes
DPADD+=		${LIBDES}
.endif

SRCS+=		${IPSEC_SRCS} ${X509} ${POLICY} ${EC} ${AGGRESSIVE} ${DNSSEC} \
		${ISAKMP_CFG} ${PRIVSEP} ${DPD} ${NAT_TRAVERSAL}
CFLAGS+=	${IPSEC_CFLAGS} ${DNSSEC_CFLAGS}

LDADD+=		${DESLIB} ${LWRESLIB}
DPADD+=		${DESLIBDEP} ${LWRESLIB}

exchange_num.c exchange_num.h: genconstants.sh exchange_num.cst
	/bin/sh ${.CURDIR}/genconstants.sh ${.CURDIR}/exchange_num

ipsec_fld.c ipsec_fld.h: genfields.sh ipsec_fld.fld
	/bin/sh ${.CURDIR}/genfields.sh ${.CURDIR}/ipsec_fld

ipsec_num.c ipsec_num.h: genconstants.sh ipsec_num.cst
	/bin/sh ${.CURDIR}/genconstants.sh ${.CURDIR}/ipsec_num

isakmp_fld.c isakmp_fld.h: genfields.sh isakmp_fld.fld
	/bin/sh ${.CURDIR}/genfields.sh ${.CURDIR}/isakmp_fld

isakmp_num.c isakmp_num.h: genconstants.sh isakmp_num.cst
	/bin/sh ${.CURDIR}/genconstants.sh ${.CURDIR}/isakmp_num

${PROG} beforedepend: ${GENERATED}

.include <bsd.prog.mk>
.include <bsd.subdir.mk>

debug:
	(cd ${.CURDIR}; ${MAKE} DEBUG="-g -Werror")
