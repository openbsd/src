#    $OpenBSD: GNUmakefile,v 1.11 2005/02/10 15:00:36 ho Exp $

#
# Copyright (c) 1998, 1999, 2000 Niklas Hallqvist.  All rights reserved.
# Copyright (c) 2000 Håkan Olsson.  All rights reserved.
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
# This makefile is a GNU makefile, which is generally available on most
# systems, either as "make" or (often) "gmake". It has been converted from
# a 'pmake' makefile (OpenBSDs 'make'), and some care has been taken to
# produce similar behaviour.
#

# openbsd means 2.5 or newer, freeswan is the name for Linux with FreeS/WAN
# integrated, freebsd/netbsd means FreeBSD/NetBSD with KAME IPsec.
# darwin means MacOS X 10.2 and later with KAME IPsec. linux means Linux-2.5
# and later with native IPSec support.
OS=		openbsd
#OS=		netbsd
#OS=		freebsd
#OS=		freeswan
#OS=		darwin
#OS=		linux

.CURDIR:=	$(shell pwd)
VPATH=		${.CURDIR}/sysdep/${OS}

PROG=		isakmpd

ifndef BINDIR
BINDIR=		/sbin
endif
ifndef LDSTATIC
LDSTATIC=	-static
endif

SRCS=		app.c attribute.c cert.c connection.c \
		constants.c conf.c cookie.c crypto.c dh.c doi.c exchange.c \
		exchange_num.c field.c gmp_util.c hash.c if.c ike_auth.c \
		ike_main_mode.c ike_phase_1.c ike_quick_mode.c init.c \
		ipsec.c ipsec_fld.c ipsec_num.c isakmpd.c isakmp_doi.c \
		isakmp_fld.c isakmp_num.c key.c libcrypto.c log.c message.c \
		math_2n.c math_group.c prf.c sa.c sysdep.c timer.c \
		transport.c udp.c ui.c util.c virtual.c

GENERATED=	exchange_num.h ipsec_fld.h ipsec_num.h isakmp_fld.h \
		isakmp_num.h
CLEANFILES=	exchange_num.c exchange_num.h ipsec_num.c ipsec_num.h \
		isakmp_num.c isakmp_num.h ipsec_fld.c ipsec_fld.h \
		isakmp_fld.c isakmp_fld.h
MAN=		isakmpd.8 isakmpd.conf.5 isakmpd.policy.5

CFLAGS+=	-O2 ${DEBUG} -Wall -DNEED_SYSDEP_APP \
		-I${.CURDIR} -I${.CURDIR}/sysdep/${OS} -I. \

# Different debugging & profiling suggestions

# Include symbolic debugging info
DEBUG=		-g

# Do execution time profiles
#CFLAGS+=	-pg

# If you have ElectricFence available, you can spot abuses of the heap.
# (/usr/ports/devel/ElectricFence)
#LDADD+=		-L/usr/local/lib -lefence
#DPADD+=		/usr/local/lib/libefence.a

# If you like to use Boehm's garbage collector (/usr/ports/devel/boehm-gc).
#LDADD+=		-L/usr/local/lib -lgc
#DPADD+=		/usr/local/lib/libgc.a

# You can also use Boehm's garbage collector as a means to find leaks.
# XXX The defines below are GCC-specific.  I think it is OK to require
# XXX GCC if you are debugging isakmpd in this way.
#LDADD+=		-L/usr/local/lib -lleak
#DPADD+=		/usr/local/lib/libleak.a
#CFLAGS+=	-D'malloc(x)=({ \
#		  void *GC_debug_malloc (size_t, char *, int); \
#		  GC_debug_malloc ((x), __FILE__, __LINE__); \
#		})' \
#		-D'realloc(x,y)=({ \
#		  void *GC_debug_realloc (void *, size_t, char *, int); \
#		  GC_debug_realloc ((x), (y), __FILE__, __LINE__); \
#		})' \
#		-D'free(x)=({ \
#		  void GC_debug_free (void *); \
#		  GC_debug_free (x); \
#		})' \
#		-D'calloc(x,y)=malloc((x) * (y))' \
#		-D'strdup(x)=({ \
#		  char *strcpy (char *, const char *); \
#		  const char *_x_ = (x); \
#		  char *_y_ = malloc (strlen (_x_) + 1); \
#		  strcpy (_y_, _x_); \
#		})'

# Ignore any files with these names...
.PHONY:	mksubdirs all clean cleandir cleandepend beforedepend \
	afterdepend realclean realcleandepend

# Default target, it needs to be the first target in makefile... :(

all: ${PROG} mksubdirs

ifneq ($(findstring install,$(MAKECMDGOALS)),install)
# Skip 'regress' until the regress/ structure has gmake makefiles for it.
#SUBDIR:=	regress
SUBDIR:=
mksubdirs:
	$(foreach DIR, ${SUBDIR}, \
		cd ${DIR}; ${MAKE} ${MAKEFLAGS} CFLAGS="${CFLAGS}" \
			MKDEP="${MKDEP}" ${MAKECMDGOALS})
else
mksubdirs:
endif

# DEPSRCS handling is *ugly*, I know...
# What is does; keep orig SRCS in ORIGSRCS; potentially add stuff to
# SRCS (include); let DEPSRCS be ORIGSRCS (sysdep.c -> sysdep/<os>/sysdep.c)
# _plus_ any new sources, located either in cwd or sysdep/<os>. Phew.

ORIGSRCS:=	${SRCS}
-include 	sysdep/${OS}/GNUmakefile.sysdep

FEATURES_UC=	$(shell echo ${FEATURES} | tr '[:lower:]' '[:upper:]')
CFLAGS+=	$(foreach F, ${FEATURES_UC}, -DUSE_${F})
-include	$(foreach F, ${FEATURES}, features/${F})

ifdef USE_KEYNOTE
USE_LIBCRYPTO=	yes
LDADD+=		-lkeynote -lm
DPADD+=		${LIBKEYNOTE} ${LIBM}
POLICY=		policy.c
CFLAGS+=	-DUSE_KEYNOTE
endif

ifdef USE_LIBCRYPTO
X509=		x509.c
CFLAGS+=	-DUSE_LIBCRYPTO
LDADD+=		-lcrypto
DPADD+=		${LIBCRYPTO}
endif

ifdef USE_RAWKEY
USE_LIBCRYPTO=  yes
CFLAGS+=        -DUSE_RAWKEY
endif

SRCS+=		${IPSEC_SRCS} ${X509} ${POLICY} ${EC} ${AGGRESSIVE} ${DNSSEC} \
		${ISAKMP_CFG} ${NAT_TRAVERSAL} ${DPD}
CFLAGS+=	${IPSEC_CFLAGS}
LDADD+=		${DESLIB}
DPADD+=		${DESLIBDEP}

DEPSRCS:=	$(subst sysdep.c,${VPATH}/sysdep.c,${ORIGSRCS}) \
		$(foreach FILE, $(filter-out ${ORIGSRCS},${SRCS}), \
			$(wildcard ./${FILE} ${VPATH}/${FILE}))
OBJS:=		$(SRCS:%.c=%.o)

# Generated targets
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

# Program rules
${PROG} beforedepend: ${GENERATED}

${PROG}: ${OBJS} ${DPADD}
	${CC} ${DEBUG} ${LDFLAGS} ${LDSTATIC} -o $@ ${OBJS} ${LDADD}

# Depend rules
depend: beforedepend .depend mksubdirs afterdepend
	@true

# Since 'mkdep' et al maybe doesn't exist...
MKDEP:= ${CC} -MM

.depend: ${SRCS}
	@rm -f .depend
	${MKDEP} ${CFLAGS} ${DEPSRCS} > .depend

afterdepend:

ifneq ($(findstring clean, $(MAKECMDGOALS)), clean)
# This will initially fail (when .depend does not exist), continue
# to create .depend, then make will automatically restart to include
# the generated .depend correctly. The '-' inhibits the warning msg.
-include .depend
endif

# Clean rules

cleandir: realclean realcleandepend mksubdirs

clean: realclean mksubdirs

cleandepend: realcleandepend mksubdirs

realclean:
	rm -f a.out core *.core ${PROG} ${OBJS} ${CLEANFILES}

realcleandepend:
	rm -f .depend tags
