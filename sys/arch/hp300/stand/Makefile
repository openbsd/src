#	$NetBSD: Makefile,v 1.19 1996/01/15 01:41:37 thorpej Exp $

#	@(#)Makefile	8.1 (Berkeley) 6/10/93

NOPROG=	noprog
NOMAN=	noman

SUBDIR=	libsa

# RELOC=FFF00000 allows for boot prog up to FF000 (1044480) bytes long
RELOC=	FFF00000

CONS=	-DDCACONSOLE -DITECONSOLE -DDCMCONSOLE
DEFS=	-DSTANDALONE -DCOMPAT_NOLABEL ${CONS} -Dhp300 # -DROMPRF
#DEFS+=	-DNETIF_DEBUG -DRPC_DEBUG -DNFS_DEBUG -DRARP_DEBUG -DNET_DEBUG
#DEFS+=	-DLE_DEBUG
CFLAGS=	-O3 ${INCPATH} ${DEFS}

SRCS=   autoconf.c cons.c devopen.c machdep.c prf.c
OBJS=	${SRCS:N*.h:R:S/$/.o/g}
DSRCS=	ct.c dca.c dcm.c fhpib.c hil.c hpib.c if_le.c \
	ite.c ite_dv.c ite_gb.c ite_rb.c ite_subr.c ite_tc.c ite_hy.c \
	nhpib.c rd.c scsi.c sd.c
DOBJS=	${DSRCS:N*.h:R:S/$/.o/g}

S=	${.CURDIR}/../../..

.PATH: ${S}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}
.PATH: ${S}/stand

INCPATH=-I${.CURDIR} -I${.CURDIR}/../.. -I${S} -I${S}/lib/libsa
### find out what to use for libkern
KERNREL=
.include "$S/lib/libkern/Makefile.inc"
LIBKERN=	${KERNLIB}

.include "${.CURDIR}/libsa/Makefile.inc"
LIBSA=	${SA_LIB}

LIBS=	${OBJS} libdrive.a ${LIBSA} ${LIBKERN}

BOOTS=	pboot netboot sys_inst dboot dcopy tboot tcopy cat #ls
ALL=	${BOOTS} mkboot installboot

all: ${ALL}

${BOOTS}: ${LIBS}

libdrive.a: ${DOBJS}
	rm -f $@
	${AR} cq $@ ${DOBJS}
	${RANLIB} $@

# depend on DEFS

devopen.o machdep.o srt0.o: Makefile
cons.o dca.o hil.o: Makefile
ite.o ite_subr.o ite_dv.o ite_gb.o ite_hy.o ite_rb.o ite_tc.o: Makefile

# startups

srt0.o: ${.CURDIR}/srt0.s
	${CC} ${INCPATH} ${DEFS} -c ${.CURDIR}/srt0.s

tpsrt0.o: ${.CURDIR}/srt0.s
	${CC} ${INCPATH} ${DEFS} -DTP -c ${.CURDIR}/srt0.s -o $@

# new boot
pboot:	srt0.o pboot.o tgets.o conf.o ${LIBS}
	${LD} -N -T ${RELOC} -e begin srt0.o pboot.o tgets.o conf.o ${LIBS} -o $@
	@size $@
	@echo $@ total size should not exceed 1044480 bytes

ls:	srt0.o ls.o conf.o ${LIBS}
	${LD} -N -T ${RELOC} -e begin srt0.o ls.o conf.o ${LIBS} -o $@
	@size $@
	@echo $@ total size should not exceed 1044480 bytes

cat:	srt0.o cat.o conf.o ${LIBS}
	${LD} -N -T ${RELOC} -e begin srt0.o cat.o conf.o ${LIBS} -o $@
	@size $@
	@echo $@ total size should not exceed 1044480 bytes

# installation
sys_inst: srt0.o sys_inst.o clock.o instconf.o inst_netio.o tgets.o ${LIBS}
	${LD} -N -T ${RELOC} -e begin srt0.o sys_inst.o clock.o instconf.o inst_netio.o tgets.o ${LIBS} -o $@
	@size $@
	@echo $@ total size should not exceed 1044480 bytes

instconf.o: ${.CURDIR}/conf.c
	${CC} ${CFLAGS} -DSYS_INST -c ${.CURDIR}/conf.c -o $@

inst_netio.o: ${.CURDIR}/netio.c
	${CC} ${CFLAGS} -DSYS_INST -c ${.CURDIR}/netio.c -o $@

# bootable from network

netboot:	srt0.o netboot.o clock.o netconf.o netio.o tgets.o ${LIBS}
	${LD} -N -T ${RELOC} -e begin srt0.o netboot.o clock.o netconf.o netio.o tgets.o ${LIBS} -o $@
	@size $@
	@echo $@ total size should not exceed 1044480 bytes

netconf.o: ${.CURDIR}/conf.c
	${CC} ${CFLAGS} -DNETBOOT -c ${.CURDIR}/conf.c -o $@

# bootable from tape

tboot:	srt0.o tboot.o conf.o ${LIBS}
	${LD} -N -T ${RELOC} -e begin srt0.o tboot.o conf.o ${LIBS} -o $@
	@size $@
	@echo $@ total size should not exceed 1044480 bytes

tboot.o: ${.CURDIR}/boot.c
	${CC} ${CFLAGS} -DJUSTASK -c ${.CURDIR}/boot.c -o $@

tcopy:	tpsrt0.o copy.o conf.o ${LIBS}
	${LD} -N -T ${RELOC} -e begin tpsrt0.o copy.o conf.o ${LIBS} -o $@
	@size $@
	@echo $@ total size should not exceed 1044480 bytes

# bootable from floppy or real disks

dboot:	srt0.o boot.o tapeconf.o ${LIBS}
	${LD} -N -T ${RELOC} -e begin srt0.o boot.o tapeconf.o ${LIBS} -o $@
	@size $@
	@echo $@ text+data size should not exceed 57344 bytes
	@echo $@ total size should not exceed 1044480 bytes

tapeconf.o: ${.CURDIR}/conf.c
	${CC} ${CFLAGS} -DTAPEBOOT -c ${.CURDIR}/conf.c -o $@

dcopy:	srt0.o copy.o conf.o ${LIBS}
	${LD} -N -T ${RELOC} -e begin srt0.o copy.o conf.o ${LIBS} -o $@

mkboot: ${.CURDIR}/mkboot.c
	${CC} ${CFLAGS} ${.CURDIR}/mkboot.c -o $@

installboot: ${.CURDIR}/installboot.sh
	@rm -f installboot
	cp -p ${.CURDIR}/installboot.sh installboot

# utilities

clean::
	rm -f *.o *.exe *.i errs make.out
	rm -f a.out pboot cat ls tboot tcopy
	rm -f boot[a-z]? boot[a-wyz][a-z].c conf[a-wyz][a-z].c
	rm -f core sboot
	rm -f libdrive.a mkboot dboot dcopy installboot *.lif

install: mkboot installboot ${ALL}
	./mkboot pboot netbsdboot.lif
	./mkboot netboot netboot.lif
	./mkboot sys_inst sys_inst.lif
	./mkboot dboot diskboot.lif
	./mkboot tboot tcopy tapeboot.lif
	install -c -o ${BINOWN} -g ${BINGRP} -m 555 installboot \
	    ${DESTDIR}/usr/mdec
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 netbsdboot.lif \
	    ${DESTDIR}/usr/mdec/rdboot
	rm -f ${DESTDIR}/usr/mdec/bootrd
	ln ${DESTDIR}/usr/mdec/rdboot ${DESTDIR}/usr/mdec/bootrd
	rm -f ${DESTDIR}/usr/mdec/sdboot
	ln ${DESTDIR}/usr/mdec/rdboot ${DESTDIR}/usr/mdec/sdboot
	rm -f ${DESTDIR}/usr/mdec/bootsd
	ln ${DESTDIR}/usr/mdec/sdboot ${DESTDIR}/usr/mdec/bootsd
	install -d -m 755 -o ${BINOWN} -g ${BINGRP} ${DESTDIR}/usr/mdec/rbootd
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 netboot.lif \
	    ${DESTDIR}/usr/mdec/rbootd/SYS_NBOOT
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 sys_inst.lif \
	    ${DESTDIR}/usr/mdec/rbootd/SYS_INST
	install -c -o ${BINOWN} -g ${BINGRP} -m 644 tapeboot.lif \
	    ${DESTDIR}/usr/mdec/bootct

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
