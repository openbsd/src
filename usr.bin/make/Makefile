#	$OpenBSD: Makefile,v 1.24 2000/11/24 14:36:33 espie Exp $

PROG=	make
CFLAGS+= -I${.OBJDIR} -I${.CURDIR} -Wall -Wno-char-subscripts -Wstrict-prototypes

CFLAGS+=-DUSE_TIMESPEC
.if (${MACHINE_ARCH} == "m88k")
CFLAGS+=-O0
.endif

SRCS=	arch.c buf.c compat.c cond.c dir.c error.c for.c job.c lowparse.c \
	main.c make.c parse.c str.c suff.c targ.c var.c varmodifiers.c #util.c 
SRCS+=	lstAppend.c lstAtEnd.c lstAtFront.c lstClose.c lstConcat.c \
	lstConcatDestroy.c lstDeQueue.c lstDestroy.c lstDupl.c lstEnQueue.c \
	lstFindFrom.c lstForEachFrom.c \
	lstInit.c lstInsert.c lstIsAtEnd.c lstIsEmpty.c \
	lstMember.c lstNext.c lstOpen.c lstRemove.c lstReplace.c lstSucc.c
.PATH:	${.CURDIR}/lst.lib


make: libohash.a
.PATH:  ${.CURDIR}/ohash
CFLAGS+=-I${.CURDIR}/ohash
LDADD=-L${.OBJDIR} -lohash

LIBOBJS= hash_create_entry.o hash_delete.o hash_do.o hash_entries.o \
    hash_enum.o hash_init.o hash_interval.o \
    hash_lookup_interval.o hash_lookup_memory.o hash_lookup_string.o \
    hash_qlookup.o hash_qlookupi.o

libohash.a: ${LIBOBJS}
	rm -f $@
	ar cq $@ `lorder ${LIBOBJS}|tsort`
	ranlib $@

CLEANFILES+=generate hashconsts.h generate.o regress.o check

CLEANFILES+=${LIBOBJS} libohash.a
CLEANFILES+= hashconsts.h generate.o generate

beforedepend: hashconsts.h
# may need tweaking if you add variable synonyms or change the hash function
MAGICSLOTS=34

hashconsts.h: generate
	${.OBJDIR}/generate ${MAGICSLOTS} >${.TARGET}

generate: generate.o error.o libohash.a
	${CC} -o ${.TARGET} ${CFLAGS} ${.ALLSRC} ${LDADD}

check: regress.o str.o error.o buf.o libohash.a
	${CC} -o ${.TARGET} ${CFLAGS} ${.ALLSRC} ${LDADD}

regress: check
	${.OBJDIR}/check 

# kludge for people who forget to make depend
var.o: hashconsts.h

.if make(install)
SUBDIR+= PSD.doc
.endif

.include <bsd.prog.mk>
