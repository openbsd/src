#	$OpenBSD: Makefile,v 1.60 1999/02/26 00:54:00 deraadt Exp $
#	$NetBSD: Makefile,v 1.59 1996/05/15 21:36:43 jtc Exp $

#	@(#)Makefile	5.45.1.1 (Berkeley) 5/6/91

# The ``rm -rf''s used below are safe because rm doesn't follow symbolic
# links.

# Missing: mp.h

FILES=	a.out.h ar.h assert.h bitstring.h blf.h bm.h cast.h cpio.h ctype.h \
	curses.h db.h dbm.h des.h dirent.h disktab.h elf_abi.h err.h fnmatch.h \
	fstab.h fts.h glob.h grp.h ieeefp.h inttypes.h iso646.h kvm.h \
	langinfo.h libgen.h limits.h locale.h malloc.h math.h md4.h md5.h \
	memory.h mpool.h ndbm.h netdb.h netgroup.h nlist.h nl_types.h \
	olf_abi.h paths.h poll.h pwd.h ranlib.h re_comp.h regex.h resolv.h \
	rmd160.h search.h setjmp.h sgtty.h sha1.h skipjack.h signal.h stab.h \
	stddef.h stdio.h stdlib.h string.h strings.h struct.h sysexits.h \
	tar.h time.h ttyent.h tzfile.h unistd.h utime.h utmp.h vis.h

.if (${MACHINE_ARCH} != "alpha") && (${MACHINE_ARCH} != "mips")
FILES+=	dlfcn.h link.h
.endif

MFILES=	float.h frame.h stdarg.h varargs.h
LFILES=	errno.h fcntl.h syslog.h termios.h

.if (${MACHINE_ARCH} == "mips")
MFILES+= link.h dlfcn.h
.endif

DIRS=	arpa protocols rpc rpcsvc
LDIRS=	dev net netinet netinet6 netccitt netiso netns netipx nfs sys ufs vm ddb \
	scsi netatalk isofs xfs

RDIRS=	../lib/libc_r ../lib/libcom_err ../lib/libcompat ../lib/libcurses \
        ../lib/libform ../lib/libmenu ../lib/libocurses ../lib/libossaudio \
	../lib/libpanel ../lib/librpcsvc ../lib/libskey ../lib/libedit \
	../lib/libpcap ../lib/libutil ../lib/libwrap ../lib/libz \
	../sys/arch/${MACHINE}

WDIRS=	../lib/libssl ../gnu/lib/libg++/libg++ ../gnu/lib/libg++/libio \
	../gnu/lib/libg++/librx ../gnu/lib/libg++/libstdc++ \
	../gnu/usr.bin/gcc ../gnu/lib/libgmp

NOOBJ=	noobj

# Change SYS_INCLUDE in bsd.own.mk to "symlinks" if you don't want copies
.include <bsd.own.mk>
SYS_INCLUDE?=	copies
.if (${KERBEROS} == "yes")
RDIRS+=	../kerberosIV/include ../kerberosIV/kadm ../kerberosIV/krb
.endif

includes:
	@echo installing ${FILES}
	@-for i in ${FILES}; do \
		cmp -s $$i ${DESTDIR}/usr/include/$$i || \
		    ${INSTALL} ${INSTALL_COPY} -m 444 $$i ${DESTDIR}/usr/include/$$i; \
	done
	@echo installing ${DIRS}
	@-for i in ${DIRS}; do \
		${INSTALL} -d -o ${BINOWN} -g ${BINGRP} -m 755 \
			${DESTDIR}/usr/include/$$i; \
		(cd $$i; for j in *.[ih]; do \
			cmp -s $$j ${DESTDIR}/usr/include/$$i/$$j || \
			${INSTALL} ${INSTALL_COPY} -m 444 $$j ${DESTDIR}/usr/include/$$i/$$j; \
		done); \
	done
	@echo installing ${LFILES}
	@-for i in ${LFILES}; do \
		rm -f ${DESTDIR}/usr/include/$$i; \
		ln -s sys/$$i ${DESTDIR}/usr/include/$$i; \
	done
	@echo installing ${MFILES}
	@-for i in ${MFILES}; do \
		rm -f ${DESTDIR}/usr/include/$$i; \
		ln -s machine/$$i ${DESTDIR}/usr/include/$$i; \
	done
	chown -R ${BINOWN}:${BINGRP} ${DESTDIR}/usr/include
	find ${DESTDIR}/usr/include -type f | \
		xargs chmod a=r
	find ${DESTDIR}/usr/include -type d | \
		xargs chmod u=rwx,go=rx
	@-for i in ${RDIRS}; do \
		echo installing in ${.CURDIR}/$$i; \
		(cd ${.CURDIR}/$$i; ${MAKE} includes) \
	done
	@-for i in ${WDIRS}; do \
		echo installing in ${.CURDIR}/$$i; \
		(cd ${.CURDIR}/$$i; ${MAKE} -f Makefile.bsd-wrapper includes) \
	done

copies:
	@echo copies: ${LDIRS}
	@-for i in ${LDIRS}; do \
		rm -rf ${DESTDIR}/usr/include/$$i; \
		${INSTALL} -d -o ${BINOWN} -g ${BINGRP} -m 755 \
			${DESTDIR}/usr/include/$$i ; \
	done
	cd ../sys; \
	pax -rw -pa -L \
	    `find ${LDIRS} -follow -type f -name '*.h' '!' -path \
	    'netiso/xebec/*' -print` ${DESTDIR}/usr/include
	rm -rf ${DESTDIR}/usr/include/machine
	${INSTALL} -d -o ${BINOWN} -g ${BINGRP} -m 755 \
		${DESTDIR}/usr/include/machine
	pax -rw -pa -s "|\.\./sys/arch/${MACHINE}/include||" \
	    ../sys/arch/${MACHINE}/include/*.h \
	    ${DESTDIR}/usr/include/machine   
	rm -rf ${DESTDIR}/usr/include/${MACHINE_ARCH}
	if test ${MACHINE} != ${MACHINE_ARCH} -a \
	    -d ../sys/arch/${MACHINE_ARCH}/include; then \
		${INSTALL} -d -o ${BINOWN} -g ${BINGRP} -m 755 \
    	    	    ${DESTDIR}/usr/include/${MACHINE_ARCH}; \
		pax -rw -pa -s "|\.\./sys/arch/${MACHINE_ARCH}/include||" \
		    ../sys/arch/${MACHINE_ARCH}/include/*.h \
		    ${DESTDIR}/usr/include/${MACHINE_ARCH}; \
	else \
		ln -s machine ${DESTDIR}/usr/include/${MACHINE_ARCH}; \
	fi

symlinks:
	@echo symlinks: ${LDIRS}
	@for i in ${LDIRS}; do \
		rm -rf ${DESTDIR}/usr/include/$$i; \
		ln -s /sys/$$i ${DESTDIR}/usr/include/$$i; \
	done
	rm -rf ${DESTDIR}/usr/include/machine
	ln -s /sys/arch/${MACHINE}/include ${DESTDIR}/usr/include/machine
	rm -rf ${DESTDIR}/usr/include/${MACHINE_ARCH}
	if test ${MACHINE} != ${MACHINE_ARCH} -a \
	    -d ../sys/arch/${MACHINE_ARCH}/include ; then \
		ln -s /sys/arch/${MACHINE_ARCH}/include \
		    ${DESTDIR}/usr/include/${MACHINE_ARCH} ; \
	else \
		ln -s machine ${DESTDIR}/usr/include/${MACHINE_ARCH} ; \
	fi

includes: ${SYS_INCLUDE}

.include <bsd.prog.mk>
