
PROGS = 		vcpu
CFLAGS =		-O2 -Wall

REGRESS_TARGETS +=	run-regress-vcpu
run-regress-vcpu: vcpu
	${SUDO} ./vcpu

.include <bsd.regress.mk>
