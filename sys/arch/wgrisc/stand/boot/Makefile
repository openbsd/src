#	$OpenBSD: Makefile,v 1.2 1997/07/21 06:58:11 pefo Exp $

PROG=boot
SADIR=${.CURDIR}/..
SRCS=start.S boot.c filesystem.c conf.c sd.c fl.c
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

${PROG}: $(OBJS) $(LDADD)
	$(LD) $(LDFLAGS) -o $(PROG) $(OBJS) $(LDADD)

${PROG}.bin: ${PROG} 
	cp ${PROG} ${PROG}.tmp
	strip -s ${PROG}.tmp
	dd if=${PROG}.tmp of=${PROG}.bin bs=512 skip=8 count=1
	dd if=/dev/zero of=${PROG}.bin bs=512 seek=1 count=1
	dd if=${PROG}.tmp of=${PROG}.bin bs=512 skip=9 seek=2 count=14
	rm ${PROG}.tmp

.include <bsd.prog.mk>
