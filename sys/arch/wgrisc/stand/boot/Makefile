#	$OpenBSD: Makefile,v 1.1 1997/05/11 16:17:51 pefo Exp $

PROG=boot
SADIR=${.CURDIR}/..
SRCS=start.S boot.c filesystem.c conf.c sd.c
CFLAGS+=$(SACFLAGS) -I${.CURDIR}/../../../../lib/libsa -I${.CURDIR}/../libsa
CFLAGS+=-D__INTERNAL_LIBSA_CREAD

LD=ld
LDFLAGS+= -T ld.script -x -e __start
LDADD=  ${LIBSA}

all:    machine-links boot.bin

machine-links:
	@rm -f machine wgrisc
	@ln -fs ${.CURDIR}/../.. wgrisc
	@ln -fs ${.CURDIR}/../../include machine

${PROG}: $(OBJS) $(DPADD)
	$(LD) $(LDFLAGS) -o $(PROG) $(OBJS) $(LDADD)

${PROG}.bin: ${PROG}
	cp ${PROG} ${PROG}.tmp
	strip -s ${PROG}.tmp
	dd if=${PROG}.tmp of=${PROG}.bin bs=4096 skip=1
	rm ${PROG}.tmp

.include <bsd.prog.mk>
