#	$OpenBSD: Makefile,v 1.17 2000/06/23 16:27:29 espie Exp $

PROG=	make
CFLAGS+= -I${.OBJDIR} -I${.CURDIR} -Wall -Wno-char-subscripts -Wno-unused -Wstrict-prototypes#-Wmissing-prototypes -Wstrict-prototypes

.if (${MACHINE_ARCH} == "m88k")
CFLAGS+=-O0
.endif

SRCS=	arch.c buf.c compat.c cond.c dir.c error.c for.c hash.c job.c main.c \
	make.c parse.c str.c suff.c targ.c var.c #util.c 
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

CLEANFILES+=${LIBOBJS} libohash.a

beforedepend: hashconsts.h
# may need tweaking if you add variable synonyms or change the hash function
MAGICSLOTS=34

hashconsts.h: generate
	${.OBJDIR}/generate ${MAGICSLOTS} >${.TARGET}

generate: generate.o error.o libohash.a
	${CC} -o ${.TARGET} ${CFLAGS} ${.ALLSRC} ${LDADD}

# kludge for people who forget to make depend
${.CURDIR}/var.c: hashconsts.h

.if make(install)
SUBDIR+= PSD.doc
.endif

.include <bsd.prog.mk>
