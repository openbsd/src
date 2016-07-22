# $OpenBSD: Makefile,v 1.2 2016/07/22 09:28:59 reyk Exp $

# The following ports must be installed for the regression tests:
# p5-Net-Pcap		Perl interface for libpcap
# p5-NetPacket		Perl interface for packet encoding/decoding
# p5-Crypt-Random	To fill payloads with weak random data
#
# Check wether all required perl packages are installed.  If some
# are missing print a warning and skip the tests, but do not fail.

PERL_REQUIRE !=	perl -Mstrict -Mwarnings -e ' \
    eval { require NetPacket::Ethernet } or print $@; \
    eval { require Net::Pcap } or print $@; \
    eval { require Crypt::Random } or print $@; \
'
.if ! empty (PERL_REQUIRE)
regress:
	@echo "${PERL_REQUIRE}"
	@echo install these perl packages for additional tests
.endif

# Automatically generate regress targets from test cases in directory.

ARGS !=			cd ${.CURDIR} && ls args-*.pm
TARGETS ?=		${ARGS}
REGRESS_TARGETS =	${TARGETS:S/^/run-regress-/}
CLEANFILES +=		*.log  ktrace.out stamp-*
CLEANFILES +=		*.h *.ph

SRC_PATH =		${.CURDIR}/../../../usr.sbin/switchd
SYS_PATH =		${.CURDIR}/../../../sys
OFP_HEADERS =		ofp.h ofp10.h
OFP_PERLHEADERS =	${OFP_HEADERS:S/.h/.ph/}

# Set variables so that make runs with and without obj directory.
# Only do that if necessary to keep visible output short.

.if ${.CURDIR} == ${.OBJDIR}
PERLINC =
PERLPATH =
.else
PERLINC =	-I${.CURDIR}
PERLPATH =	${.CURDIR}/
.endif

# The arg tests take a perl hash with arguments controlling the
# test parameters.  Generally they consist of switch, switchd.

.for a in ${ARGS}
run-regress-$a: $a
	@echo '\n======== $@ ========'
	time SUDO=${SUDO} KTRACE=${KTRACE} SWITCHD=${SWITCHD} perl ${PERLINC} ${PERLPATH}run.pl ${PERLPATH}$a
.endfor

${OFP_HEADERS}:
	@-mkdir -p ${.OBJDIR}/net
	@-for i in ${SRC_PATH}/$@ ${SYS_PATH}/$@; do \
		test -s $$i && grep -v '^#include' $$i > ${.OBJDIR}/$@; \
	done

.SUFFIXES: .h .ph
.h.ph:
	@h2ph -d ${.OBJDIR} $<

${REGRESS_TARGETS:M*}: ${OFP_PERLHEADERS}

# make perl syntax check for all args files

.PHONY: syntax

syntax: stamp-syntax

stamp-syntax: ${ARGS}
.for a in ${ARGS}
	@perl -c ${PERLPATH}$a
.endfor
	@date >$@

.include <bsd.regress.mk>
