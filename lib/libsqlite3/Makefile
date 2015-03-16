# $OpenBSD: Makefile,v 1.13 2015/03/16 00:12:37 jturner Exp $

.include <bsd.own.mk>

.if defined(NOPIC)
CPPFLAGS +=	-DSQLITE_OMIT_LOAD_EXTENSION=1
.endif

CPPFLAGS +=	-I${.OBJDIR} -I${.CURDIR}/tsrc -I${.CURDIR}/src \
		-I${.CURDIR}/ext/rtree -I${.CURDIR}/ext/fts3
LIB = sqlite3

.PATH: ${.CURDIR}/tsrc ${.CURDIR}/src ${.CURDIR}/ext/fts3 ${.CURDIR}/ext/rtree

SRCS =	alter.c analyze.c attach.c auth.c \
	backup.c bitvec.c btmutex.c btree.c build.c \
	callback.c complete.c ctime.c date.c delete.c \
	expr.c fault.c fkey.c \
	fts3.c fts3_aux.c fts3_expr.c fts3_hash.c fts3_icu.c fts3_porter.c \
	fts3_snippet.c fts3_tokenize_vtab.c fts3_tokenizer.c fts3_tokenizer1.c \
	fts3_unicode.c fts3_unicode2.c fts3_write.c \
	func.c global.c hash.c \
	insert.c journal.c legacy.c loadext.c \
	main.c malloc.c mem1.c \
	memjournal.c \
	mutex.c mutex_noop.c mutex_unix.c \
	notify.c opcodes.c os.c os_unix.c \
	pager.c parse.c pcache.c pcache1.c pragma.c prepare.c printf.c \
	random.c resolve.c rowset.c rtree.c select.c status.c \
	table.c threads.c okenize.c trigger.c \
	update.c util.c vacuum.c \
	vdbe.c vdbeapi.c vdbeaux.c vdbeblob.c vdbemem.c vdbesort.c \
	vdbetrace.c wal.c walker.c where.c utf.c vtab.c

# so that it works with NO THREADS
SRCS +=	pthread_stub.c

#	mem3.c mem5.c

FEATURE_FLAGS =	-DSQLITE_ENABLE_COLUMN_METADATA \
		-DSQLITE_ENABLE_RTREE \
		-DSQLITE_ENABLE_UNLOCK_NOTIFY \
		-DSQLITE_ENABLE_FTS3 \
		-DSQLITE_ENABLE_FTS3_PARENTHESIS \
		-DSQLITE_OMIT_BUILTIN_TEST

CPPFLAGS += 	$(FEATURE_FLAGS) -DSQLITE_THREADSAFE=1 \
		-DHAVE_STDINT_H=1 -DHAVE_INTTYPES_H=1 \
		-DHAVE_GMTIME_R=1 \
		-DHAVE_LOCALTIME_R=1 \
		-DHAVE_USLEEP=1 \
		-DHAVE_ARC4RANDOM_BUF=1 \
		-DHAVE_STRERROR_R=1 \
		-DSQLITE_TEMP_STORE=1 \
		-DSQLITE_SOUNDEX=1 \
		-DSQLITE_HAVE_ISNAN=1 \
		-DSQLITE_OS_UNIX=1 \
		-DSQLITE_CORE \
		-DHAVE_UINT32_T=1 -DHAVE_UINT16_T=1 -DHAVE_INT16_T=1 \
		-DHAVE_UINT8_T=1 -DHAVE_INT8_T=1

FILES = sqlite3.h sqlite3ext.h

opcodes.c: opcodes.h ${.CURDIR}/mkopcodec.awk
	sort -n -b -k 3 opcodes.h | awk -f ${.CURDIR}/mkopcodec.awk >opcodes.c


opcodes.h: parse.h ${.CURDIR}/src/vdbe.c \
    ${.CURDIR}/mkopcodeh.awk
	cat parse.h ${.CURDIR}/src/vdbe.c | \
	    awk -f ${.CURDIR}/mkopcodeh.awk >$@

beforedepend: opcodes.h keywordhash.h

keywordhash.h: mkkeywordhash
	${.OBJDIR}/mkkeywordhash >$@

mkkeywordhash: tool/mkkeywordhash.c
	${HOSTCC} ${LDSTATIC} -o $@ $< ${LDADD}

lemon: tool/lemon.c
	${HOSTCC} ${LDSTATIC} -o $@ $< ${LDADD}

parse.c: parse.y lemon
	ln -sf ${.CURDIR}/src/parse.y
	ln -sf ${.CURDIR}/src/lempar.c # XXX tweaked parser
	${.OBJDIR}/lemon ${FEATURE_FLAGS} parse.y
	mv parse.h parse.h.temp
	awk -f ${.CURDIR}/addopcodes.awk parse.h.temp >parse.h
	rm parse.h.temp

parse.h: parse.c

tokenize.o tokenize.po tokenize.so: keywordhash.h

CLEANFILES += mkkeywordhash opcodes.c opcodes.h keywordhash.h \
	parse.* lemon lempar.c
	
beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o root -g ${SHAREGRP} \
	    -m ${SHAREMODE} ${.CURDIR}/sqlite3.pc ${DESTDIR}/usr/lib/pkgconfig/

includes:
	@for i in ${FILES}; do \
	    cmp -s ${.CURDIR}/src/$$i ${DESTDIR}/usr/include/$$i || \
		${INSTALL} ${INSTALL_COPY} -m 444 ${.CURDIR}/src/$$i ${DESTDIR}/usr/include/$$i; \
	done

.PHONY: header
header:
	cd ${.CURDIR} && perl tsrc/header_regen VERSION src/sqlite.h.in src/sqlite3.h sqlite3.pc.in sqlite3.pc

.include <bsd.lib.mk>

${OBJS} ${GOBJS} ${POBJS} ${SOBJS}: opcodes.h parse.h
