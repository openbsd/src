#	$OpenBSD: Makefile,v 1.4 2013/08/07 00:38:41 bluhm Exp $


PERL_REQUIRE !=	perl -Mstrict -Mwarnings -e ' \
    eval { require BSD::Socket::Splice } or print $@; \
    eval { require IO::Socket::INET6 } or print $@; \
    eval { require Socket6 } or print $@; \
'

SUBDIR +=	perf
.if empty(PERL_REQUIRE)
SUBDIR +=	error
SUBDIR +=	tcp
SUBDIR +=	udp
.else
all: _SUBDIRUSE
	@echo "${PERL_REQUIRE}"
	@echo install these perl packages for additional tests
.endif

.include <bsd.subdir.mk>
