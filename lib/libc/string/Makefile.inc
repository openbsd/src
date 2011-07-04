#	$OpenBSD: Makefile.inc,v 1.26 2011/07/04 04:37:34 nicm Exp $

# string sources
.PATH: ${LIBCSRCDIR}/arch/${MACHINE_CPU}/string ${LIBCSRCDIR}/string

SRCS+=	bm.c memccpy.c memrchr.c strcasecmp.c strcasestr.c strcoll.c strdup.c \
	strerror.c strerror_r.c strlcat.c strmode.c strndup.c strnlen.c \
	strsignal.c strtok.c strxfrm.c \
	wcscat.c wcschr.c wcscmp.c wcscpy.c wcscspn.c wcslcat.c wcslcpy.c \
	wcslen.c wcsncat.c wcsncmp.c wcsncpy.c wcspbrk.c wcsrchr.c wcsspn.c \
	wcsstr.c wcstok.c wcswcs.c wcswidth.c wmemchr.c wmemcmp.c wmemcpy.c \
	wmemmove.c wmemset.c wcsdup.c \
	timingsafe_bcmp.c wcscasecmp.c

# machine-dependent net sources
# m-d Makefile.inc must include sources for:
#	bcmp() bcopy() bzero() ffs() index() memchr() memcmp() memset()
#	rindex() strcat() strcmp() strcpy() strcspn() strlen() strlcpy()
#	strncat() strncmp() strncpy() strpbrk() strsep()
#	strspn() strstr() swav()
# m-d Makefile.inc may include sources for:
#	memcpy() memmove() strchr() strrchr()

.include "${LIBCSRCDIR}/arch/${MACHINE_CPU}/string/Makefile.inc"

# if no machine specific memmove(3), build one out of bcopy(3).
.if empty(SRCS:Mmemmove.S)
OBJS+=	memmove.o
memmove.o: bcopy.c
	${CC} -DMEMMOVE ${CFLAGS} ${CPPFLAGS} -c ${.ALLSRC} -o ${.TARGET}
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

memmove.go: bcopy.c
	${CC} -g -DMEMMOVE ${CFLAGS} ${CPPFLAGS} -c ${.ALLSRC} -o ${.TARGET}
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

memmove.po: bcopy.c
	${CC} -DMEMMOVE ${CFLAGS} ${CPPFLAGS} -c -p ${.ALLSRC} -o ${.TARGET}
	@${LD} -o ${.TARGET}.tmp -X -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

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
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

memcpy.go: bcopy.c
	${CC} -g -DMEMCOPY ${CFLAGS} ${CPPFLAGS} -c ${.ALLSRC} -o ${.TARGET}
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

memcpy.po: bcopy.c
	${CC} -DMEMCOPY ${CFLAGS} ${CPPFLAGS} -c -p ${.ALLSRC} -o ${.TARGET}
	@${LD} -o ${.TARGET}.tmp -X -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

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
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

strchr.go: index.c
	${CC} -g -DSTRCHR ${CFLAGS} ${CPPFLAGS} -c ${.ALLSRC} -o ${.TARGET}
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

strchr.po: index.c
	${CC} -DSTRCHR ${CFLAGS} ${CPPFLAGS} -c -p ${.ALLSRC} -o ${.TARGET}
	@${LD} -o ${.TARGET}.tmp -X -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

strchr.so: index.c
	${CC} ${PICFLAG} -DPIC -DSTRCHR ${CFLAGS} ${CPPFLAGS} -c ${.ALLSRC} \
	    -o ${.TARGET}
.endif

# if no machine specific strrchr(3), build one out of rindex(3).
.if empty(SRCS:Mstrrchr.S)
OBJS+=	strrchr.o
strrchr.o: rindex.c
	${CC} -DSTRRCHR ${CFLAGS} ${CPPFLAGS} -c ${.ALLSRC} -o ${.TARGET}
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

strrchr.go: rindex.c
	${CC} -g -DSTRRCHR ${CFLAGS} ${CPPFLAGS} -c ${.ALLSRC} -o ${.TARGET}
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

strrchr.po: rindex.c
	${CC} -DSTRRCHR ${CFLAGS} ${CPPFLAGS} -c -p ${.ALLSRC} -o ${.TARGET}
	@${LD} -o ${.TARGET}.tmp -X -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

strrchr.so: rindex.c
	${CC} ${PICFLAG} -DPIC -DSTRRCHR ${CFLAGS} ${CPPFLAGS} -c ${.ALLSRC} \
	    -o ${.TARGET}
.endif

# build .ln files for memmove, memcpy, strchr and strrchr always from
# bcopy, index, and rindex
LOBJS+=	memmove.ln memcpy.ln strchr.ln strrchr.ln

memmove.ln: bcopy.c
	lint ${LINTFLAGS} -DMEMMOVE ${CFLAGS:M-[IDU]*} -i -o ${.TARGET} \
		${LIBCSRCDIR}/string/bcopy.c

memcpy.ln: bcopy.c
	lint ${LINTFLAGS} -DMEMCOPY ${CFLAGS:M-[IDU]*} -i -o ${.TARGET} \
		${LIBCSRCDIR}/string/bcopy.c

strchr.ln: index.c
	lint ${LINTFLAGS} -DSTRCHR ${CFLAGS:M-[IDU]*} -i -o ${.TARGET} \
		${LIBCSRCDIR}/string/index.c

strrchr.ln: rindex.c
	lint ${LINTFLAGS} -DSTRRCHR ${CFLAGS:M-[IDU]*} -i -o ${.TARGET} \
		${LIBCSRCDIR}/string/rindex.c

MAN+=	bm.3 bcmp.3 bcopy.3 bstring.3 bzero.3 ffs.3 memccpy.3 memchr.3 \
	memcmp.3 memcpy.3 memmove.3 memset.3 strcasecmp.3 strcat.3 \
	strchr.3 strcmp.3 strcoll.3 strcpy.3 strcspn.3 strerror.3 \
	string.3 strlen.3 strmode.3 strdup.3 strpbrk.3 strrchr.3 strsep.3 \
	strsignal.3 strspn.3 strstr.3 strtok.3 strxfrm.3 swab.3 strlcpy.3 \
	wcstok.3 wmemchr.3 wcswidth.3 wcsdup.3

MLINKS+=bm.3 bm_comp.3 bm.3 bm_exec.3 bm.3 bm_free.3
MLINKS+=memchr.3 memrchr.3
MLINKS+=strchr.3 index.3
MLINKS+=strrchr.3 rindex.3
MLINKS+=strcasecmp.3 strncasecmp.3
MLINKS+=strcat.3 strncat.3
MLINKS+=strcmp.3 strncmp.3
MLINKS+=strcpy.3 strncpy.3
MLINKS+=strdup.3 strndup.3
MLINKS+=strlcpy.3 strlcat.3
MLINKS+=strlen.3 strnlen.3
MLINKS+=strstr.3 strcasestr.3
MLINKS+=strtok.3 strtok_r.3
MLINKS+=strerror.3 strerror_r.3
MLINKS+=wmemchr.3 wmemcmp.3
MLINKS+=wmemchr.3 wmemcpy.3
MLINKS+=wmemchr.3 wmemmove.3
MLINKS+=wmemchr.3 wmemset.3
MLINKS+=wmemchr.3 wcscat.3
MLINKS+=wmemchr.3 wcschr.3
MLINKS+=wmemchr.3 wcscmp.3
MLINKS+=wmemchr.3 wcscasecmp.3
MLINKS+=wmemchr.3 wcscpy.3
MLINKS+=wmemchr.3 wcscspn.3
MLINKS+=wmemchr.3 wcslcat.3
MLINKS+=wmemchr.3 wcslcpy.3
MLINKS+=wmemchr.3 wcslen.3
MLINKS+=wmemchr.3 wcsncat.3
MLINKS+=wmemchr.3 wcsncmp.3
MLINKS+=wmemchr.3 wcsncasecmp.3
MLINKS+=wmemchr.3 wcsncpy.3
MLINKS+=wmemchr.3 wcspbrk.3
MLINKS+=wmemchr.3 wcsrchr.3
MLINKS+=wmemchr.3 wcsspn.3
MLINKS+=wmemchr.3 wcsstr.3
MLINKS+=bcmp.3 timingsafe_bcmp.3
