#	$OpenBSD: Makefile,v 1.37 2004/01/28 02:01:48 mickey Exp $

PROG=	make
CFLAGS+= -I${.OBJDIR} -I${.CURDIR}
CDIAGFLAGS=-Wall -W -Wno-char-subscripts -Wstrict-prototypes -pedantic \
	-Wmissing-prototypes

CFLAGS+=-DUSE_TIMESPEC
CFLAGS+=-DHAS_BOOL_H
CFLAGS+=-DHAS_PATHS_H
CFLAGS+=-DHAS_EXTENDED_GETCWD
#CFLAGS+=-DHAS_STATS

SRCS=	arch.c buf.c cmd_exec.c compat.c cond.c dir.c error.c for.c \
	init.c job.c lowparse.c main.c make.c memory.c parse.c \
	parsevar.c str.c stats.c suff.c targ.c timestamp.c \
	var.c varmodifiers.c varname.c
SRCS+=	lstAddNew.c lstAppend.c lstConcat.c lstConcatDestroy.c \
	lstDeQueue.c lstDestroy.c lstDupl.c lstFindFrom.c lstForEachFrom.c \
	lstInsert.c lstMember.c lstRemove.c lstReplace.c lstSucc.c
.PATH:	${.CURDIR}/lst.lib

# Use this for bootstrapping
#.PATH: ${.CURDIR}/../../lib/libc/ohash
#CFLAGS+=-I${.CURDIR}/../../lib/libc/ohash
#HSRCS+=	ohash_create_entry.c ohash_delete.c ohash_do.c ohash_entries.c \
#	ohash_enum.c ohash_init.c ohash_interval.c \
#	ohash_lookup_interval.c ohash_lookup_memory.c \
#	ohash_qlookup.c ohash_qlookupi.c
#SRCS+=	${HSRCS}

CLEANFILES+=generate hashconsts.h generate.o regress.o check

CLEANFILES+=${LIBOBJS} libohash.a
CLEANFILES+= varhashconsts.h condhashconsts.h generate.o generate

beforedepend: varhashconsts.h condhashconsts.h
# may need tweaking if you add variable synonyms or change the hash function
MAGICVARSLOTS=77
MAGICCONDSLOTS=65

varhashconsts.h: generate
	${.OBJDIR}/generate 1 ${MAGICVARSLOTS} >${.TARGET}

condhashconsts.h: generate
	${.OBJDIR}/generate 2 ${MAGICCONDSLOTS} >${.TARGET}

generate: generate.c stats.c memory.c ${HSRCS}
	${HOSTCC} ${LDSTATIC} -o ${.TARGET} ${CFLAGS} ${.ALLSRC} ${LDADD}

check: regress.o str.o memory.o buf.o
	${CC} -o ${.TARGET} ${CFLAGS} ${.ALLSRC} ${LDADD}

regress: check
	${.OBJDIR}/check 

# kludge for people who forget to make depend
var.o: varhashconsts.h
cond.o: condhashconsts.h

.if make(install)
SUBDIR+= PSD.doc
.endif

.PHONY:		regress

.include <bsd.prog.mk>
