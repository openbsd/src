#	$OpenBSD: Makefile,v 1.1.1.1 2007/07/06 00:43:38 jakemsr Exp $

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
	./audiotest_rw -o master.pcm


# recording tests

run-regress-record: ${PROG}
	./audiotest_rw -o test.pcm
	@test -s test.pcm || (echo "no output" && false)

run-regress-record-duplex: ${PROG}
	./audiotest_rw -o test.pcm -d
	@test -s test.pcm || (echo "no output" && false)

run-regress-record-poll: ${PROG}
	./audiotest_rw -o test.pcm -p
	@test -s test.pcm || (echo "no output" && false)

run-regress-record-select: ${PROG}
	./audiotest_rw -o test.pcm -s
	@test -s test.pcm || (echo "no output" && false)

run-regress-record-poll-duplex: ${PROG}
	./audiotest_rw -o test.pcm -p -d
	@test -s test.pcm || (echo "no output" && false)

run-regress-record-select-duplex: ${PROG}
	./audiotest_rw -o test.pcm -s -d
	@test -s test.pcm || (echo "no output" && false)


# playing tests

run-regress-play: ${PROG} master.pcm
	./audiotest_rw -i master.pcm

run-regress-play-duplex: ${PROG} master.pcm
	./audiotest_rw -i master.pcm -d

run-regress-play-poll: ${PROG} master.pcm
	./audiotest_rw -i master.pcm -p

run-regress-play-select: ${PROG} master.pcm
	./audiotest_rw -i master.pcm -s

run-regress-play-poll-duplex: ${PROG} master.pcm
	./audiotest_rw -i master.pcm -p -d

run-regress-play-select-duplex: ${PROG} master.pcm
	./audiotest_rw -i master.pcm -s -d


# full-duplex tests

run-regress-duplex: ${PROG} master.pcm
	./audiotest_rw -i master.pcm -o test.pcm
	@test -s test.pcm || (echo "no output" && false)

run-regress-duplex-poll: ${PROG} master.pcm
	./audiotest_rw -i master.pcm -o test.pcm -p
	@test -s test.pcm || (echo "no output" && false)

run-regress-duplex-select: ${PROG} master.pcm
	./audiotest_rw -i master.pcm -o test.pcm -s
	@test -s test.pcm || (echo "no output" && false)


.include <bsd.regress.mk>
