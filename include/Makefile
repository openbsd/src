#	$OpenBSD: Makefile,v 1.79 2000/02/25 17:37:43 hin Exp $
#	$NetBSD: Makefile,v 1.59 1996/05/15 21:36:43 jtc Exp $

#	@(#)Makefile	5.45.1.1 (Berkeley) 5/6/91

# The ``rm -rf''s used below are safe because rm doesn't follow symbolic
# links.

# Missing: mp.h

FILES=	a.out.h ar.h assert.h bitstring.h blf.h bm.h cast.h cpio.h ctype.h \
	curses.h db.h dbm.h des.h dirent.h disktab.h elf_abi.h err.h fnmatch.h \
	fstab.h fts.h glob.h grp.h ieeefp.h ifaddrs.h inttypes.h iso646.h \
	kvm.h langinfo.h libgen.h limits.h locale.h malloc.h math.h md4.h \
	md5.h memory.h mpool.h ndbm.h netdb.h netgroup.h nlist.h nl_types.h \
	olf_abi.h paths.h poll.h pwd.h ranlib.h re_comp.h regex.h resolv.h \
	rmd160.h search.h setjmp.h sgtty.h sha1.h skipjack.h signal.h stab.h \
	stdbool.h stddef.h stdio.h stdlib.h string.h strings.h struct.h sysexits.h \
	tar.h time.h ttyent.h tzfile.h unistd.h utime.h utmp.h vis.h

.if (${MACHINE_ARCH} != "alpha") && (${MACHINE_ARCH} != "mips") && \
	(${MACHINE_ARCH} != "powerpc")
FILES+=	dlfcn.h link.h
.endif

MFILES=	float.h frame.h stdarg.h varargs.h
LFILES=	errno.h fcntl.h syslog.h termios.h

.if (${MACHINE_ARCH} == "mips")
MFILES+= link.h dlfcn.h
.endif

DIRS=	arpa protocols rpc rpcsvc
LDIRS=	dev net netinet netinet6 netccitt netiso netns netipx nfs sys ufs vm \
	ddb scsi netatalk isofs xfs uvm miscfs

# Directories with an includes target
RDIRS=	../lib/libc_r ../lib/libcom_err ../lib/libcompat ../lib/libcurses \
	../lib/libcurses++ ../lib/libform ../lib/libmenu ../lib/libocurses \
	../lib/libossaudio ../lib/libpanel ../lib/librpcsvc ../lib/libskey \
	../lib/libedit ../lib/libpcap ../lib/libutil ../lib/libwrap \
	../lib/libz ../lib/libkeynote ../sys/arch/${MACHINE}

# Places using Makefile that needs a prerequisite target met before includes
PRDIRS=

# Directories with an includes target that use Makefile.bsd-wrapper
WDIRS=	../lib/libssl ../gnu/lib/libgmp ../usr.sbin/httpd
WDIRS+= ../gnu/egcs/libio ../gnu/egcs/libstdc++ ../gnu/egcs/libf2c \
	../gnu/egcs/libobjc ../gnu/egcs/gcc

# Places using Makefile.bsd-wrapper that needs a prerequisite target met
# before includes
PWDIRS=	../lib/libssl ../usr.sbin/httpd ../gnu/egcs/libf2c

NOOBJ=	noobj

# Change SYS_INCLUDE in bsd.own.mk to "symlinks" if you don't want copies
.include <bsd.own.mk>
SYS_INCLUDE?=	copies
.if (${KERBEROS} == "yes")
RDIRS+=	../kerberosIV
.endif

prereq:
	@-for i in ${PRDIRS}; do \
		echo preparing in ${.CURDIR}/$$i; \
		(cd ${.CURDIR}/$$i; ${MAKE} prereq) \
	done
	@-for i in ${PWDIRS}; do \
		echo preparing in ${.CURDIR}/$$i; \
		(cd ${.CURDIR}/$$i; ${MAKE} -f Makefile.bsd-wrapper prereq) \
	done

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
	@rm -f ${DESTDIR}/usr/include/openssl
	@ln -sf ssl ${DESTDIR}/usr/include/openssl
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
	    'netiso/xebec/*' '!' -path 'dev/microcode/*' \
	    -print` ${DESTDIR}/usr/include
	cd ${DESTDIR}/usr/include && rm -rf ${MACHINE} ${MACHINE_ARCH} machine
	${INSTALL} -d -o ${BINOWN} -g ${BINGRP} -m 755 \
		${DESTDIR}/usr/include/${MACHINE}
	pax -rw -pa -s "|\.\./sys/arch/${MACHINE}/include||" \
	    ../sys/arch/${MACHINE}/include/*.h \
	    ${DESTDIR}/usr/include/${MACHINE}
	if test ${MACHINE} != ${MACHINE_ARCH} -a \
	    -d ../sys/arch/${MACHINE_ARCH}/include; then \
		${INSTALL} -d -o ${BINOWN} -g ${BINGRP} -m 755 \
    	    	    ${DESTDIR}/usr/include/${MACHINE_ARCH}; \
		pax -rw -pa -s "|\.\./sys/arch/${MACHINE_ARCH}/include||" \
		    ../sys/arch/${MACHINE_ARCH}/include/*.h \
		    ${DESTDIR}/usr/include/${MACHINE_ARCH}; \
	fi
	ln -sf ${MACHINE} ${DESTDIR}/usr/include/machine; \

symlinks:
	@echo symlinks: ${LDIRS}
	@for i in ${LDIRS}; do \
		rm -rf ${DESTDIR}/usr/include/$$i; \
		ln -s /sys/$$i ${DESTDIR}/usr/include/$$i; \
	done
	cd ${DESTDIR}/usr/include && rm -rf ${MACHINE} ${MACHINE_ARCH} machine
	ln -s /sys/arch/${MACHINE}/include ${DESTDIR}/usr/include/${MACHINE}
	if test ${MACHINE} != ${MACHINE_ARCH} -a \
	    -d ../sys/arch/${MACHINE_ARCH}/include ; then \
		ln -s /sys/arch/${MACHINE_ARCH}/include \
		    ${DESTDIR}/usr/include/${MACHINE_ARCH} ; \
	fi
	ln -sf ${MACHINE} ${DESTDIR}/usr/include/machine ; \

includes: ${SYS_INCLUDE}

.include <bsd.prog.mk>
