#	$OpenBSD: Makefile,v 1.9 1998/12/22 02:09:46 niklas Exp $
#	$EOM: Makefile,v 1.37 1998/12/22 02:09:14 niklas Exp $

#
# Copyright (c) 1998 Niklas Hallqvist.  All rights reserved.
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

PROG=		isakmpd
BINDIR?=	/sbin
SRCS=		app.c asn.c asn_useful.c attribute.c cert.c constants.c \
		conf.c cookie.c crypto.c dh.c doi.c exchange.c exchange_num.c \
		field.c gmp_util.c hash.c if.c ike_auth.c ike_main_mode.c \
		ike_quick_mode.c init.c ipsec.c ipsec_fld.c ipsec_num.c \
		isakmpd.c isakmp_doi.c isakmp_fld.c isakmp_num.c log.c \
		message.c math_2n.c math_ec2n.c math_group.c pf_encap.c \
		pkcs.c prf.c sa.c sysdep.c timer.c transport.c udp.c ui.c \
		util.c x509.c
GENERATED=	exchange_num.h ipsec_fld.h ipsec_num.h isakmp_fld.h \
		isakmp_num.h
CLEANFILES=	exchange_num.c exchange_num.h ipsec_num.c ipsec_num.h \
		isakmp_num.c isakmp_num.h ipsec_fld.c ipsec_fld.h \
		isakmp_fld.c isakmp_fld.h
MAN=		isakmpd.8 isakmpd.conf.5
LDADD=		-lgmp -ldes
DPADD=		${LIBGMP} ${LIBDES}
CFLAGS+=	-Wall -I. -I${.CURDIR} -DNEED_SYSDEP_APP

# Different debugging & profiling suggestions
#
# Include symbolic debugging info
DEBUG=		-g
#
# Do execution time profiles
#CFLAGS+=	-pg
#
# If you have ElectricFence available, you can spot abuses of the heap.
#LDADD+=		-L/usr/local/lib -lefence
#

.if !make(install)
SUBDIR=		regress
.endif

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
