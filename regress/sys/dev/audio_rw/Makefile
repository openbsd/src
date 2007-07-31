#	$OpenBSD: Makefile,v 1.2 2007/07/31 03:14:09 jakemsr Exp $

PROG=	audiotest_rw
CFLAGS+=-Wall -Wstrict-prototypes -Wmissing-prototypes
MAN1=	audiotest_rw.1

.ifndef DO_AUTEST
REGRESS_SKIP =
.else
REGRESS_TARGETS =
REGRESS_TARGETS +=	run-regress-record
REGRESS_TARGETS +=	run-regress-record-duplex
REGRESS_TARGETS +=	run-regress-record-poll
REGRESS_TARGETS +=	run-regress-record-select
REGRESS_TARGETS +=	run-regress-record-poll-duplex
REGRESS_TARGETS +=	run-regress-record-select-duplex
REGRESS_TARGETS +=	run-regress-play
REGRESS_TARGETS +=	run-regress-play-duplex
REGRESS_TARGETS +=	run-regress-play-poll
REGRESS_TARGETS +=	run-regress-play-select
REGRESS_TARGETS +=	run-regress-play-poll-duplex
REGRESS_TARGETS +=	run-regress-play-select-duplex
REGRESS_TARGETS +=	run-regress-duplex
REGRESS_TARGETS +=	run-regress-duplex-poll
REGRESS_TARGETS +=	run-regress-duplex-select
.endif


# audio data file for playing tests

master.pcm: ${PROG}
	@echo "creating master input file"
	./audiotest_rw -o master.pcm ${OPTS}


# recording tests

run-regress-record: ${PROG}
	./audiotest_rw -o test.pcm  ${OPTS}
	@test -s test.pcm || (echo "no output" && false)

run-regress-record-duplex: ${PROG}
	./audiotest_rw -o test.pcm -d  ${OPTS}
	@test -s test.pcm || (echo "no output" && false)

run-regress-record-poll: ${PROG}
	./audiotest_rw -o test.pcm -p  ${OPTS}
	@test -s test.pcm || (echo "no output" && false)

run-regress-record-select: ${PROG}
	./audiotest_rw -o test.pcm -s ${OPTS}
	@test -s test.pcm || (echo "no output" && false)

run-regress-record-poll-duplex: ${PROG}
	./audiotest_rw -o test.pcm -p -d ${OPTS}
	@test -s test.pcm || (echo "no output" && false)

run-regress-record-select-duplex: ${PROG}
	./audiotest_rw -o test.pcm -s -d ${OPTS}
	@test -s test.pcm || (echo "no output" && false)


# playing tests

run-regress-play: ${PROG} master.pcm
	./audiotest_rw -i master.pcm ${OPTS}

run-regress-play-duplex: ${PROG} master.pcm
	./audiotest_rw -i master.pcm -d ${OPTS}

run-regress-play-poll: ${PROG} master.pcm
	./audiotest_rw -i master.pcm -p ${OPTS}

run-regress-play-select: ${PROG} master.pcm
	./audiotest_rw -i master.pcm -s ${OPTS}

run-regress-play-poll-duplex: ${PROG} master.pcm
	./audiotest_rw -i master.pcm -p -d ${OPTS}

run-regress-play-select-duplex: ${PROG} master.pcm
	./audiotest_rw -i master.pcm -s -d ${OPTS}


# full-duplex tests

run-regress-duplex: ${PROG} master.pcm
	./audiotest_rw -i master.pcm -o test.pcm ${OPTS}
	@test -s test.pcm || (echo "no output" && false)

run-regress-duplex-poll: ${PROG} master.pcm
	./audiotest_rw -i master.pcm -o test.pcm -p ${OPTS}
	@test -s test.pcm || (echo "no output" && false)

run-regress-duplex-select: ${PROG} master.pcm
	./audiotest_rw -i master.pcm -o test.pcm -s ${OPTS}
	@test -s test.pcm || (echo "no output" && false)


.include <bsd.regress.mk>
