# $OpenBSD: Makefile,v 1.3 2021/01/09 15:39:37 bluhm Exp $

# Create chains and loops of spliced tcp and udp sockets.
# Send data through them and check that MAXLOOP is triggered in kernel.

TARGETS =
VERBOSE ?=

v =
.if !empty(VERBOSE) && ${VERBOSE:L} != "no"
v =	-v
.endif

.for i in 4 6
.for p in tcp udp
.for c in 1 2 3 127 128 129 200
REGRESS_TARGETS +=	run-chain-ipv$i-proto$p-count$c
run-chain-ipv$i-proto$p-count$c: chain.pl
	ulimit -n 500; \
	SUDO=${SUDO} ${KTRACE} perl ${PERLPATH}chain.pl -$i -c $c -p $p $v
.endfor

REGRESS_TARGETS +=	run-loop-ipv$i-proto$p
run-loop-ipv$i-proto$p: loop.pl
	SUDO=${SUDO} ${KTRACE} perl ${PERLPATH}loop.pl -$i -p $p $v
.endfor
.endfor

REGRESS_TARGETS +=	run-bcast
run-bcast: bcast.pl
	SUDO=${SUDO} ${KTRACE} perl ${PERLPATH}bcast.pl $v

.include <bsd.regress.mk>
