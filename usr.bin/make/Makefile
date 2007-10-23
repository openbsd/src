#	$OpenBSD: Makefile,v 1.48 2007/10/23 22:04:24 miod Exp $

PROG=	make
CFLAGS+= -I${.OBJDIR} -I${.CURDIR}
HOSTCFLAGS+= -I${.OBJDIR} -I${.CURDIR}
CDIAGFLAGS=-Wall -W -Wno-char-subscripts -Wstrict-prototypes -pedantic \
	-Wmissing-prototypes

CDEFS+=-DUSE_TIMESPEC
CDEFS+=-DHAS_BOOL_H
CDEFS+=-DHAS_PATHS_H
CDEFS+=-DHAS_EXTENDED_GETCWD
#CDEFS+=-DHAS_STATS

CFLAGS+=${CDEFS}
HOSTCFLAGS+=${CDEFS}

SRCS=	arch.c buf.c cmd_exec.c compat.c cond.c dir.c direxpand.c engine.c \
	error.c for.c init.c job.c lowparse.c main.c make.c memory.c parse.c \
	parsevar.c str.c stats.c suff.c targ.c timestamp.c \
	var.c varmodifiers.c varname.c
SRCS+=	lstAddNew.c lstAppend.c lstConcat.c lstConcatDestroy.c \
	lstDeQueue.c lstDestroy.c lstDupl.c lstFindFrom.c lstForEachFrom.c \
	lstInsert.c lstMember.c lstRemove.c lstReplace.c lstSucc.c
.PATH:	${.CURDIR}/lst.lib

CLEANFILES+=generate generate.o regress.o check

CLEANFILES+=${LIBOBJS} libohash.a
CLEANFILES+= varhashconsts.h condhashconsts.h nodehashconsts.h

beforedepend: varhashconsts.h condhashconsts.h nodehashconsts.h
# may need tweaking if you add variable synonyms or change the hash function
MAGICVARSLOTS=77
MAGICCONDSLOTS=65

varhashconsts.h: generate
	${.OBJDIR}/generate 1 ${MAGICVARSLOTS} >${.TARGET}

condhashconsts.h: generate
	${.OBJDIR}/generate 2 ${MAGICCONDSLOTS} >${.TARGET}

nodehashconsts.h: generate
	${.OBJDIR}/generate 3 0 >${.TARGET}

generate: generate.c stats.c memory.c
	${HOSTCC} ${LDSTATIC} -o ${.TARGET} ${HOSTCFLAGS} ${.ALLSRC} ${LDADD}

check: regress.o str.o memory.o buf.o
	${CC} -o ${.TARGET} ${CFLAGS} ${.ALLSRC} ${LDADD}

regress: check
	${.OBJDIR}/check 

# kludge for people who forget to make depend
var.o: varhashconsts.h
cond.o: condhashconsts.h
targ.o parse.o: nodehashconsts.h
var.ln: varhashconsts.h
cond.ln: condhashconsts.h
targ.ln parse.ln: nodehashconsts.h

.if make(install)
SUBDIR+= PSD.doc
.endif

.PHONY:		regress

.include <bsd.prog.mk>
