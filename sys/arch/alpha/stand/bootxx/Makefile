#	$NetBSD: Makefile,v 1.4 1995/11/23 02:40:29 cgd Exp $

.PATH: ${.CURDIR}/.. ${.CURDIR}/../../../../lib/libsa

BOOT_PROG = bootxx
BOOT_RELOC = ${PRIMARY_LOAD_ADDRESS}

BOOT_SRCS = start.S bootxx.c prom.c prom_disp.S bzero.c puts.c
BOOT_OBJS = ${BOOT_SRCS:N*.h:R:S/$/.o/g}

HEADERSIZE_PROG = headersize

AFLAGS += -DASSEMBLER
CPPFLAGS += -I${.CURDIR}/../.. -DPRIMARY_BOOTBLOCK
CFLAGS = -Werror -mno-fp-regs -g

.PATH: ${.CURDIR}/../../../../lib/libkern

all: ${BOOT_PROG}

${BOOT_PROG}: ${BOOT_OBJS} ${HEADERSIZE_PROG}
	${LD} -Ttext ${BOOT_RELOC} -N -e start -o ${BOOT_PROG}.coff \
	    ${BOOT_OBJS}
	size ${BOOT_PROG}.coff
	strip ${BOOT_PROG}.coff
	dd if=${BOOT_PROG}.coff of=${BOOT_PROG}.nohdr \
	    bs=`./${HEADERSIZE_PROG} < ${BOOT_PROG}.coff` skip=1
	dd if=${BOOT_PROG}.nohdr of=${BOOT_PROG} bs=`expr 15 \* 512` conv=sync

install:
	install -c -o bin -g bin -m 444 ${BOOT_PROG} \
	    ${DESTDIR}${BINDIR}/${BOOT_PROG}

clean: _SUBDIRUSE
	rm -f a.out [Ee]rrs mklog core *.core \
	    ${BOOT_PROG} ${BOOT_OBJS} ${CLEANFILES} \
	    ${BOOT_PROG}.coff ${BOOT_PROG}.nohdr ${HEADERSIZE_PROG}

cleandir: _SUBDIRUSE clean

.include "${.CURDIR}/../Makefile.inc"
.include <bsd.obj.mk>
.include <bsd.dep.mk>
.include <bsd.subdir.mk>
