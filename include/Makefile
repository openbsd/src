#	$OpenBSD: Makefile,v 1.22 1996/09/05 11:01:32 pefo Exp $
#	$NetBSD: Makefile,v 1.59 1996/05/15 21:36:43 jtc Exp $

#	@(#)Makefile	5.45.1.1 (Berkeley) 5/6/91

# The ``rm -rf''s used below are safe because rm doesn't follow symbolic
# links.

# Missing: mp.h

FILES=	a.out.h ar.h assert.h bitstring.h bm.h cpio.h ctype.h db.h dirent.h \
	disktab.h elf_abi.h err.h fnmatch.h fstab.h fts.h glob.h \
	grp.h ieeefp.h iso646.h kvm.h langinfo.h limits.h locale.h \
	malloc.h math.h md2.h md4.h md5.h memory.h mpool.h ndbm.h netdb.h \
	netgroup.h nlist.h nl_types.h paths.h poll.h pwd.h ranlib.h re_comp.h \
	regex.h resolv.h \
	search.h setjmp.h sgtty.h signal.h stab.h stddef.h stdio.h stdlib.h \
	string.h strings.h struct.h sysexits.h tar.h time.h ttyent.h tzfile.h \
	unistd.h utime.h utmp.h vis.h

.if (${MACHINE_ARCH} != "alpha") && (${MACHINE_ARCH} != "mips")
FILES+=	dlfcn.h link.h
.endif

MFILES=	float.h frame.h stdarg.h varargs.h
LFILES=	errno.h fcntl.h syslog.h termios.h

DIRS=	arpa protocols rpc rpcsvc
LDIRS=	dev net netinet netccitt netiso netns netipx nfs sys ufs vm ddb scsi

RDIRS=	../lib/libcompat ../lib/libcurses ../lib/libform ../lib/libmenu \
	../lib/libocurses ../lib/libpanel ../lib/librpcsvc ../lib/libtermlib \
	../lib/libedit ../lib/libpcap ../lib/libutil ../lib/libz \
	../sys/arch/${MACHINE}

NOOBJ=	noobj

# Change SYS_INCLUDE in bsd.own.mk to "symlinks" if you don't want copies
.include <bsd.own.mk>
SYS_INCLUDE?=	copies
.if defined(KERBEROS)
RDIRS+=	../kerberosIV/include ../kerberosIV/kadm ../kerberosIV/krb ../kerberosIV/ss
.endif

includes:
	@echo installing ${FILES}
	@-for i in ${FILES}; do \
		cmp -s $$i ${DESTDIR}/usr/include/$$i || \
		    install -c -m 444 $$i ${DESTDIR}/usr/include/$$i; \
	done
	@echo installing ${DIRS}
	@-for i in ${DIRS}; do \
		install -d -o ${BINOWN} -g ${BINGRP} -m 755 \
			${DESTDIR}/usr/include/$$i; \
		(cd $$i; for j in *.[ih]; do \
			cmp -s $$j ${DESTDIR}/usr/include/$$i/$$j || \
			install -c -m 444 $$j ${DESTDIR}/usr/include/$$i/$$j; \
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

copies:
	@echo copies: ${LDIRS}
	@-for i in ${LDIRS}; do \
		rm -rf ${DESTDIR}/usr/include/$$i; \
		install -d -o ${BINOWN} -g ${BINGRP} -m 755 \
			${DESTDIR}/usr/include/$$i ; \
	done
	cd ../sys; \
	pax -rw -pa -L \
	    `find ${LDIRS} -follow -type f -name '*.h' '!' -path \
	    'netiso/xebec/*' -print` ${DESTDIR}/usr/include
	rm -rf ${DESTDIR}/usr/include/machine
	install -d -o ${BINOWN} -g ${BINGRP} -m 755 \
		${DESTDIR}/usr/include/machine
	pax -rw -pa -s "|../sys/arch/${MACHINE}/include||" \
	    ../sys/arch/${MACHINE}/include/*.h \
	    ${DESTDIR}/usr/include/machine   
	rm -rf ${DESTDIR}/usr/include/${MACHINE_ARCH}
	if test ${MACHINE} != ${MACHINE_ARCH} -a \
	    -d ../sys/arch/${MACHINE_ARCH}/include; then \
		install -d -o ${BINOWN} -g ${BINGRP} -m 755 \
    	    	    ${DESTDIR}/usr/include/${MACHINE_ARCH}; \
		pax -rw -pa -s "|../sys/arch/${MACHINE_ARCH}/include||" \
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
