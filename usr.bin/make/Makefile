#	$OpenBSD: Makefile,v 1.59 2015/01/23 22:35:57 espie Exp $

PROG=	make
CFLAGS+= -I${.OBJDIR} -I${.CURDIR}
HOSTCFLAGS+= -I${.OBJDIR} -I${.CURDIR}
CDIAGFLAGS=-Wall -W -Wno-char-subscripts -Wstrict-prototypes -pedantic \
	-Wmissing-prototypes -Wdeclaration-after-statement -std=c99

CDEFS+=-DHAS_BOOL_H
CDEFS+=-DHAS_PATHS_H
CDEFS+=-DHAS_EXTENDED_GETCWD
#CDEFS+=-DHAS_STATS

DPADD += ${LIBUTIL}
LDADD += -lutil
CFLAGS+=${CDEFS}
HOSTCFLAGS+=${CDEFS}

SRCS=	arch.c buf.c cmd_exec.c compat.c cond.c dir.c direxpand.c dump.c \
	engine.c \
	error.c for.c init.c job.c lowparse.c main.c make.c memory.c parse.c \
	parsevar.c str.c stats.c suff.c targ.c targequiv.c timestamp.c \
	var.c varmodifiers.c varname.c

.include "${.CURDIR}/lst.lib/Makefile.inc"

CLEANFILES+=generate generate.o regress.o check

CLEANFILES+= varhashconsts.h condhashconsts.h nodehashconsts.h

beforedepend: varhashconsts.h condhashconsts.h nodehashconsts.h
# may need tweaking if you add variable synonyms or change the hash function
MAGICVARSLOTS=77
MAGICCONDSLOTS=65

varhashconsts.h: generate
	${.OBJDIR}/generate 1 ${MAGICVARSLOTS} >$@.tmp && mv $@.tmp $@

condhashconsts.h: generate
	${.OBJDIR}/generate 2 ${MAGICCONDSLOTS} >$@.tmp && mv $@.tmp $@

nodehashconsts.h: generate
	${.OBJDIR}/generate 3 0 >$@.tmp && mv $@.tmp $@

generate: generate.c stats.c memory.c ${DPADD}
	${HOSTCC} ${HOSTCFLAGS} ${LDSTATIC} -o ${.TARGET} ${.ALLSRC} ${LDFLAGS} ${LDADD}

CHECKOBJS = regress.o str.o memory.o buf.o

check: ${CHECKOBJS} ${DPADD}
	${CC} -o ${.TARGET} ${CFLAGS} ${CHECKOBJS} ${LDADD}

regress: check
	${.OBJDIR}/check

# kludge for people who forget to make depend
var.o: varhashconsts.h
cond.o: condhashconsts.h
targ.o parse.o: nodehashconsts.h

.PHONY:		regress

.include <bsd.prog.mk>
