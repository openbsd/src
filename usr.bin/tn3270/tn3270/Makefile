#	$OpenBSD: Makefile,v 1.15 2005/06/28 20:56:37 martin Exp $

.include "../../Makefile.inc"

CFLAGS += -I${.CURDIR} -I.
CFLAGS += -I${.CURDIR}/../../telnet
LDADD += -lcurses
DPADD += ${LIBCURSES}

SRCS += apilib.c api_bsd.c api_exch.c asc_ebc.c astosc.c dctype.c
SRCS += disp_asc.c ebc_disp.c
SRCS += map3270.c termin.c
SRCS += api.c function.c inbound.c oia.c options.c outbound.c
SRCS += genbsubs.c globals.c system.c termout.c
SRCS += commands.c main.c network.c ring.c sys_bsd.c telnet.c terminal.c
SRCS += tn3270.c utilities.c
SRCS += auth.c encrypt.c genget.c misc.c

# this and the dependency hacks below to make 'depend' target
# work right...

DEPSRCS += astosc.OUT asc_disp.OUT disp_asc.OUT kbd.OUT
DEPSRCS += apilib.c api_bsd.c api_exch.c asc_ebc.c dctype.c
DEPSRCS += ebc_disp.c
DEPSRCS += map3270.c termin.c
DEPSRCS += api.c function.c inbound.c oia.c options.c outbound.c
DEPSRCS += genbsubs.c globals.c system.c termout.c
DEPSRCS += commands.c main.c network.c ring.c sys_bsd.c telnet.c terminal.c
DEPSRCS += tn3270.c utilities.c

PROG = tn3270

MKASTOSC!=cd $(.CURDIR)/../tools/mkastosc; \
	printf "xxx:\n\techo \$${.OBJDIR}/mkastosc\n" | \
	${MAKE} -r -s -f - xxx | grep mkastosc

MKASTODS!=cd $(.CURDIR)/../tools/mkastods; \
	printf "xxx:\n\techo \$${.OBJDIR}/mkastods\n" | \
	${MAKE} -r -s -f - xxx | grep mkastods

MKDSTOAS!=cd $(.CURDIR)/../tools/mkdstoas; \
	printf "xxx:\n\techo \$${.OBJDIR}/mkdstoas\n" | \
	${MAKE} -r -s -f - xxx | grep mkdstoas

MKHITS!=cd $(.CURDIR)/../tools/mkhits; \
	printf "xxx:\n\techo \$${.OBJDIR}/mkhits\n" | \
	${MAKE} -r -s -f - xxx | grep mkhits

astosc.o: astosc.OUT
CLEANFILES += astosc.OUT astosc.out
astosc.OUT: ${.CURDIR}/../ctlr/hostctlr.h ${.CURDIR}/../ctlr/function.h \
	    ${.CURDIR}/../ctlr/${KBD} ${MKASTOSC}
	${MKASTOSC} ${.CURDIR}/../ctlr/hostctlr.h \
		${.CURDIR}/../ctlr/function.h < ${.CURDIR}/../ctlr/${KBD} \
		> ${.TARGET}
	/bin/rm -f astosc.out
	ln -s astosc.OUT astosc.out

disp_asc.o: asc_disp.OUT disp_asc.OUT
CLEANFILES += asc_disp.OUT asc_disp.out disp_asc.OUT disp_asc.out
asc_disp.OUT: ${MKASTODS}
	${MKASTODS} > ${.TARGET}
	/bin/rm -f asc_disp.out
	ln -s asc_disp.OUT asc_disp.out
disp_asc.OUT: ${MKDSTOAS}
	${MKDSTOAS} > ${.TARGET}
	/bin/rm -f disp_asc.out
	ln -s disp_asc.OUT disp_asc.out

inbound.o: kbd.OUT
CLEANFILES += TMPfunc.out kbd.OUT kbd.out
kbd.OUT: ${.CURDIR}/../ctlr/hostctlr.h ${.CURDIR}/../ctlr/${KBD} \
	 ${MKHITS}
	${CC} ${CFLAGS} -E ${.CURDIR}/../ctlr/function.c > TMPfunc.out
	${MKHITS} ${.CURDIR}/../ctlr/hostctlr.h \
		TMPfunc.out < ${.CURDIR}/../ctlr/${KBD} > ${.TARGET}
	/bin/rm -f kbd.out
	ln -s kbd.OUT kbd.out

# astosc.out
# asc_disp.out disp_asc.out
# default.map
# kbd.out

${MKASTOSC}:
	cd ${.CURDIR}/../tools/mkastosc; ${MAKE}
${MKASTODS}:
	cd ${.CURDIR}/../tools/mkastods; ${MAKE}
${MKDSTOAS}:
	cd ${.CURDIR}/../tools/mkdstoas; ${MAKE}
${MKHITS}:
	cd ${.CURDIR}/../tools/mkhits; ${MAKE}


depend: .depend
.depend: ${DEPSRCS}
	mkdep ${MKDEP} ${CFLAGS:M-[ID]*} ${.ALLSRC:M*.c}

.include <bsd.prog.mk>

.PATH: ${.CURDIR}/../api ${.CURDIR}/../ascii ${.CURDIR}/../ctlr
.PATH: ${.CURDIR}/../general ${.CURDIR}/../sys_curses ${.CURDIR}/../../telnet
