#	$OpenBSD: Makefile.inc,v 1.5 1998/07/01 01:29:44 millert Exp $

# string sources
.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}/string ${.CURDIR}/string

SRCS+=	bm.c memccpy.c strcasecmp.c strcoll.c strdup.c strerror.c \
	strlcat.c strlcpy.c strmode.c strsignal.c strtok.c strxfrm.c \
	__strerror.c __strsignal.c

# machine-dependent net sources
# m-d Makefile.inc must include sources for:
#	bcmp() bcopy() bzero() ffs() index() memchr() memcmp() memset()
#	rindex() strcat() strcmp() strcpy() strcspn() strlen()
#	strncat() strncmp() strncpy() strpbrk() strsep()
#	strspn() strstr() swav()
# m-d Makefile.inc may include sources for:
#	memcpy() memmove() strchr() strrchr()

.include "${.CURDIR}/arch/${MACHINE_ARCH}/string/Makefile.inc"

# if no machine specific memmove(3), build one out of bcopy(3).
.if empty(SRCS:Mmemmove.S)
OBJS+=	memmove.o
memmove.o: bcopy.c
	${CC} -DMEMMOVE ${CFLAGS} ${CPPFLAGS} -c ${.ALLSRC} -o ${.TARGET}
	@${LD} -x -r ${.TARGET}
	@mv a.out ${.TARGET}

memmove.po: bcopy.c
	${CC} -DMEMMOVE ${CFLAGS} ${CPPFLAGS} -c -p ${.ALLSRC} -o ${.TARGET}
	@${LD} -X -r ${.TARGET}
	@mv a.out ${.TARGET}

memmove.so: bcopy.c
	${CC} ${PICFLAG} -DPIC -DMEMMOVE ${CFLAGS} ${CPPFLAGS} -c ${.ALLSRC} \
		-o ${.TARGET}
.endif

# if no machine specific memcpy(3), build one out of bcopy(3).
# if there is a machine specific memmove(3), we'll assume it aliases
# memcpy(3).
.if empty(SRCS:Mmemcpy.S)
.if empty(SRCS:Mmemmove.S)
OBJS+=	memcpy.o
memcpy.o: bcopy.c
	${CC} -DMEMCOPY ${CFLAGS} ${CPPFLAGS} -c ${.ALLSRC} -o ${.TARGET}
	@${LD} -x -r ${.TARGET}
	@mv a.out ${.TARGET}

memcpy.po: bcopy.c
	${CC} -DMEMCOPY ${CFLAGS} ${CPPFLAGS} -c -p ${.ALLSRC} -o ${.TARGET}
	@${LD} -X -r ${.TARGET}
	@mv a.out ${.TARGET}

memcpy.so: bcopy.c
	${CC} ${PICFLAG} -DPIC -DMEMCOPY ${CFLAGS} ${CPPFLAGS} -c ${.ALLSRC} \
		-o ${.TARGET}
.endif
.endif

# if no machine specific strchr(3), build one out of index(3).
.if empty(SRCS:Mstrchr.S)
OBJS+=	strchr.o
strchr.o: index.c
	${CC} -DSTRCHR ${CFLAGS} ${CPPFLAGS} -c ${.ALLSRC} -o ${.TARGET}
	@${LD} -x -r ${.TARGET}
	@mv a.out ${.TARGET}

strchr.po: index.c
	${CC} -DSTRCHR ${CFLAGS} ${CPPFLAGS} -c -p ${.ALLSRC} -o ${.TARGET}
	@${LD} -X -r ${.TARGET}
	@mv a.out ${.TARGET}

strchr.so: index.c
	${CC} ${PICFLAG} -DPIC -DSTRCHR ${CFLAGS} ${CPPFLAGS} -c ${.ALLSRC} \
	    -o ${.TARGET}
.endif

# if no machine specific strrchr(3), build one out of rindex(3).
.if empty(SRCS:Mstrrchr.S)
OBJS+=	strrchr.o
strrchr.o: rindex.c
	${CC} -DSTRRCHR ${CFLAGS} ${CPPFLAGS} -c ${.ALLSRC} -o ${.TARGET}
	@${LD} -x -r ${.TARGET}
	@mv a.out ${.TARGET}

strrchr.po: rindex.c
	${CC} -DSTRRCHR ${CFLAGS} ${CPPFLAGS} -c -p ${.ALLSRC} -o ${.TARGET}
	@${LD} -X -r ${.TARGET}
	@mv a.out ${.TARGET}

strrchr.so: rindex.c
	${CC} ${PICFLAG} -DPIC -DSTRRCHR ${CFLAGS} ${CPPFLAGS} -c ${.ALLSRC} \
	    -o ${.TARGET}
.endif

# build .ln files for memmove, memcpy, strchr and strrchr always from
# bcopy, index, and rindex
LOBJS+=	memmove.ln memcpy.ln strchr.ln strrchr.ln

memmove.ln: bcopy.c
	lint ${LINTFLAGS} -DMEMMOVE ${CFLAGS:M-[IDU]*} -i -o ${.TARGET} \
		${.CURDIR}/string/bcopy.c

memcpy.ln: bcopy.c
	lint ${LINTFLAGS} -DMEMCOPY ${CFLAGS:M-[IDU]*} -i -o ${.TARGET} \
		${.CURDIR}/string/bcopy.c

strchr.ln: index.c
	lint ${LINTFLAGS} -DSTRCHR ${CFLAGS:M-[IDU]*} -i -o ${.TARGET} \
		${.CURDIR}/string/index.c

strrchr.ln: rindex.c
	lint ${LINTFLAGS} -DSTRRCHR ${CFLAGS:M-[IDU]*} -i -o ${.TARGET} \
		${.CURDIR}/string/rindex.c

MAN+=	bm.3 bcmp.3 bcopy.3 bstring.3 bzero.3 ffs.3 index.3 memccpy.3 memchr.3 \
	memcmp.3 memcpy.3 memmove.3 memset.3 rindex.3 strcasecmp.3 strcat.3 \
	strchr.3 strcmp.3 strcoll.3 strcpy.3 strcspn.3 strerror.3 \
	string.3 strlen.3 strmode.3 strdup.3 strpbrk.3 strrchr.3 strsep.3 \
	strsignal.3 strspn.3 strstr.3 strtok.3 strxfrm.3 swab.3 strlcpy.3

MLINKS+=bm.3 bm_comp.3 bm.3 bm_exec.3 bm.3 bm_free.3
MLINKS+=strcasecmp.3 strncasecmp.3
MLINKS+=strcat.3 strncat.3
MLINKS+=strcmp.3 strncmp.3
MLINKS+=strcpy.3 strncpy.3
MLINKS+=strlcpy.3 strlcat.3
