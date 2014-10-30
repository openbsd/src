#	$OpenBSD: Makefile,v 1.5 2014/10/30 20:41:02 bluhm Exp $

PROG ?=		relay
CFLAGS +=	-Wall -Werror
CFLAGS +=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS +=	-Wmissing-declarations
CFLAGS +=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS +=	-Wsign-compare
NOMAN =		yes

TARGETS =	tcp-copy tcp-splice udp-copy udp-splice
CLEANFILES +=	copy splice

.include <bsd.regress.mk>

copy splice: ${PROG}
	ln -s relay $@

.for p in tcp udp
.for r in copy splice

run-regress-$p-$r: $r
	@echo '\n======== $@ ========'
	-pkill -x tcpbench
.if empty (REMOTE_SSH)
	./$r ${p:S/tcp//:S/udp/-u/} 12345 localhost 12346 & \
	rpid=$$! ; \
	tcpbench ${p:S/tcp//:S/udp/-u/} -t 7 -s -p 12346 & \
	sleep 1 ; \
	tcpbench ${p:S/tcp//:S/udp/-u/} -t 5 localhost ; \
	kill $$rpid ; \
	pkill -x tcpbench
.else
	ssh -f ${REMOTE_SSH} 'cd ${.CURDIR} && make PROG= LOCAL_ADDR=${LOCAL_ADDR} REMOTE_ADDR=${REMOTE_ADDR} remote-regress-$p-$r' ; \
	sleep 2 ; \
	tcpbench ${p:S/tcp//:S/udp/-u/} -t 7 -b ${LOCAL_ADDR} -s -p 12346 & \
	sleep 1 ; \
	tcpbench ${p:S/tcp//:S/udp/-u/} -t 5 ${REMOTE_ADDR} ; \
	pkill -x tcpbench
.endif
	sleep 1

remote-regress-$p-$r: $r
	-pkill -x $r
	./$r ${p:S/tcp//:S/udp/-u/} -b ${REMOTE_ADDR} 12345 ${LOCAL_ADDR} 12346 & \
	rpid=$$! ; \
	sleep 7 ; \
	kill $$rpid

.endfor
.endfor
