#	$OpenBSD: Makefile,v 1.13 2002/05/31 03:46:35 pjanzen Exp $

PROG=	hack
CFLAGS+=-I.
SRCS+=	hack.onames.h
SRCS+=	alloc.c hack.Decl.c hack.apply.c hack.bones.c hack.c hack.cmd.c \
	hack.do.c hack.do_name.c hack.do_wear.c hack.dog.c hack.eat.c \
	hack.end.c hack.engrave.c hack.fight.c hack.invent.c hack.ioctl.c \
	hack.lev.c hack.main.c hack.makemon.c hack.mhitu.c hack.mklev.c \
	hack.mkmaze.c hack.mkobj.c hack.mkshop.c hack.mon.c hack.monst.c \
	hack.o_init.c hack.objnam.c hack.options.c hack.pager.c hack.potion.c \
	hack.pri.c hack.read.c hack.rip.c hack.rumors.c hack.save.c \
	hack.search.c hack.shk.c hack.shknam.c hack.steal.c hack.termcap.c \
	hack.timeout.c hack.topl.c hack.track.c hack.trap.c hack.tty.c \
	hack.u_init.c hack.unix.c hack.vault.c hack.version.c hack.wield.c \
	hack.wizard.c hack.worm.c hack.worn.c hack.zap.c rnd.c
MAN=	hack.6
DPADD+=	${LIBCURSES}
LDADD+=	-lcurses
CLEANFILES+=hack.onames.h makedefs
BINMODE=2555

${PROG}: hack.onames.h

hack.onames.h: makedefs def.objects.h
	${.OBJDIR}/makedefs ${.CURDIR}/def.objects.h > hack.onames.h

makedefs: makedefs.c
	${HOSTCC} ${CFLAGS} ${LDFLAGS} ${LDSTATIC} -o ${.TARGET} ${.CURDIR}/${.PREFIX}.c ${LDADD}

beforeinstall: 
	${INSTALL} ${INSTALL_COPY} -o root -g games -m 660 /dev/null \
	    ${DESTDIR}/var/games/hackdir/perm
	${INSTALL} ${INSTALL_COPY} -o root -g games -m 660 /dev/null \
	    ${DESTDIR}/var/games/hackdir/record
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/help \
	    ${.CURDIR}/hh ${.CURDIR}/data ${DESTDIR}/var/games/hackdir
	rm -f ${DESTDIR}/var/games/hackdir/bones*

.include <bsd.prog.mk>
