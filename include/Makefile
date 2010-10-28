#	$OpenBSD: Makefile,v 1.157 2010/10/28 08:34:37 mikeb Exp $
#	$NetBSD: Makefile,v 1.59 1996/05/15 21:36:43 jtc Exp $

#	@(#)Makefile	5.45.1.1 (Berkeley) 5/6/91

# The ``rm -rf''s used below are safe because rm doesn't follow symbolic
# links.


.include <bsd.own.mk>

# Missing: mp.h
FILES=	a.out.h ar.h assert.h bitstring.h blf.h bm.h bsd_auth.h \
	complex.h cpio.h ctype.h curses.h db.h dbm.h des.h dirent.h disktab.h \
	dlfcn.h elf_abi.h err.h errno.h fnmatch.h fstab.h fts.h ftw.h getopt.h \
	glob.h grp.h ifaddrs.h inttypes.h iso646.h kvm.h langinfo.h \
	libgen.h limits.h locale.h login_cap.h malloc.h math.h md4.h \
	md5.h memory.h mpool.h ndbm.h netdb.h netgroup.h nlist.h nl_types.h \
	ohash.h paths.h poll.h pwd.h ranlib.h re_comp.h \
	readpassphrase.h regex.h resolv.h rmd160.h search.h setjmp.h \
	sgtty.h sha1.h sha2.h signal.h sndio.h stab.h \
	stdbool.h stddef.h stdio.h stdlib.h \
	string.h strings.h struct.h sysexits.h tar.h \
	time.h ttyent.h tzfile.h unistd.h utime.h utmp.h vis.h \
	wchar.h wctype.h

FILES+=	link.h link_aout.h link_elf.h

.if (${MACHINE_ARCH} != "vax")
FILES+= ieeefp.h
.endif

MFILES=	float.h frame.h 
LFILES=	fcntl.h syslog.h termios.h stdarg.h stdint.h varargs.h

DIRS=	arpa protocols rpc rpcsvc
LDIRS=	altq crypto ddb dev isofs miscfs msdosfs net netatalk netinet netinet6 \
	netmpls netnatm net80211 netbt nfs nnpfs ntfs scsi sys ufs uvm

# Directories with an includes target
RDIRS=	../lib/libpthread ../lib/libcompat ../lib/libcurses \
	../lib/libform ../lib/libssl ../lib/libmenu \
	../lib/libocurses ../lib/libossaudio ../lib/libpanel ../lib/librpcsvc \
	../lib/libskey ../lib/libedit ../lib/libexpat \
	../lib/libpcap ../lib/libutil ../lib/libusbhid ../lib/libwrap \
	../lib/libz ../lib/libkeynote ../lib/libevent ../usr.bin/lex \
	../gnu/lib/libreadline ../gnu/usr.sbin/sendmail/libmilter \
	../sys/arch/${MACHINE}

# Places using Makefile that needs a prerequisite target met before includes
PRDIRS=

# Directories with an includes target that use Makefile.bsd-wrapper
WDIRS=	../usr.sbin/httpd

# Places using Makefile.bsd-wrapper that needs a prerequisite target met
# before includes
PWDIRS=

.if ${COMPILER_VERSION:L} == "gcc3" 
WDIRS+= ../gnu/lib/libstdc++ ../gnu/usr.bin/gcc ../gnu/lib/libobjc
PWDIRS+= ../gnu/lib/libstdc++
.elif ${COMPILER_VERSION:L} == "gcc4"
RDIRS+= ../gnu/lib/libstdc++-v3 ../gnu/usr.bin/cc/libobjc
PRDIRS+= ../gnu/lib/libstdc++-v3
RDIRS+= ../gnu/usr.bin/cc/include
.else
WDIRS+= ../gnu/egcs/libio ../gnu/egcs/libstdc++ \
	../gnu/lib/libobjc ../gnu/egcs/gcc
.endif

NOOBJ=	noobj

# Change SYS_INCLUDE in bsd.own.mk to "symlinks" if you don't want copies
SYS_INCLUDE?=	copies
.if ${KERBEROS5:L} == "yes"
RDIRS+= ../lib/libkrb5 ../lib/libgssapi ../lib/libkadm5srv
.endif

prereq:
	@for i in ${PRDIRS}; do \
		echo preparing in ${.CURDIR}/$$i; \
		cd ${.CURDIR}/$$i && ${MAKE} prereq; \
	done
	@for i in ${PWDIRS}; do \
		echo preparing in ${.CURDIR}/$$i; \
		cd ${.CURDIR}/$$i && ${MAKE} -f Makefile.bsd-wrapper prereq; \
	done

includes:
	@echo installing ${FILES}
	@for i in ${FILES}; do \
		cmp -s $$i ${DESTDIR}/usr/include/$$i || \
		    ${INSTALL} ${INSTALL_COPY} -m 444 $$i ${DESTDIR}/usr/include/$$i; \
	done
	@echo installing ${DIRS}
	@for i in ${DIRS}; do \
		${INSTALL} -d -o ${BINOWN} -g ${BINGRP} -m 755 \
			${DESTDIR}/usr/include/$$i; \
		cd ${.CURDIR}/$$i && for j in *.[ih]; do \
			cmp -s $$j ${DESTDIR}/usr/include/$$i/$$j || \
			${INSTALL} ${INSTALL_COPY} -m 444 $$j ${DESTDIR}/usr/include/$$i/$$j; \
		done; \
	done
	@rm -rf ${DESTDIR}/usr/include/openssl ${DESTDIR}/usr/include/ssl \
		${DESTDIR}/usr/libdata/perl5/site_perl/${MACHINE_CPU}-openbsd/ssl \
		${DESTDIR}/usr/libdata/perl5/site_perl/${MACHINE_CPU}-openbsd/openssl
	@mkdir ${DESTDIR}/usr/include/openssl
	@ln -sf openssl ${DESTDIR}/usr/include/ssl
	@echo installing ${LFILES}
	@for i in ${LFILES}; do \
		rm -f ${DESTDIR}/usr/include/$$i && \
		ln -s sys/$$i ${DESTDIR}/usr/include/$$i; \
	done
	@echo installing ${MFILES}
	@for i in ${MFILES}; do \
		rm -f ${DESTDIR}/usr/include/$$i && \
		ln -s machine/$$i ${DESTDIR}/usr/include/$$i; \
	done
	chown -R ${BINOWN}:${BINGRP} ${DESTDIR}/usr/include
	find ${DESTDIR}/usr/include -type f -print0 | \
		xargs -0r chmod a=r
	find ${DESTDIR}/usr/include -type d -print0 | \
		xargs -0r chmod u=rwx,go=rx
	@for i in ${RDIRS}; do \
		echo installing in ${.CURDIR}/$$i; \
		cd ${.CURDIR}/$$i && ${MAKE} includes; \
	done
	@for i in ${WDIRS}; do \
		echo installing in ${.CURDIR}/$$i; \
		cd ${.CURDIR}/$$i && ${MAKE} -f Makefile.bsd-wrapper includes; \
	done

copies:
	@echo copies: ${LDIRS}
	@for i in ${LDIRS}; do \
		rm -rf ${DESTDIR}/usr/include/$$i && \
		${INSTALL} -d -o ${BINOWN} -g ${BINGRP} -m 755 \
			${DESTDIR}/usr/include/$$i ; \
	done
	cd ../sys; \
	pax -rw -pa -L \
	    `find ${LDIRS} -follow -type f -name '*.h' \
	    '!' -path 'dev/microcode/*' -print` ${DESTDIR}/usr/include
	cd ${DESTDIR}/usr/include && rm -rf ${MACHINE} ${MACHINE_CPU} machine
	${INSTALL} -d -o ${BINOWN} -g ${BINGRP} -m 755 \
		${DESTDIR}/usr/include/${MACHINE}
	pax -rw -pa -s "|\.\./sys/arch/${MACHINE}/include||" \
	    ../sys/arch/${MACHINE}/include/*.h \
	    ${DESTDIR}/usr/include/${MACHINE}
	if test ${MACHINE} != ${MACHINE_CPU} -a \
	    -d ../sys/arch/${MACHINE_CPU}/include; then \
		${INSTALL} -d -o ${BINOWN} -g ${BINGRP} -m 755 \
	    	    ${DESTDIR}/usr/include/${MACHINE_CPU}; \
		pax -rw -pa -s "|\.\./sys/arch/${MACHINE_CPU}/include||" \
		    ../sys/arch/${MACHINE_CPU}/include/*.h \
		    ${DESTDIR}/usr/include/${MACHINE_CPU}; \
	fi
	ln -sf ${MACHINE} ${DESTDIR}/usr/include/machine; \

symlinks:
	@echo symlinks: ${LDIRS}
	@for i in ${LDIRS}; do \
		rm -rf ${DESTDIR}/usr/include/$$i && \
		ln -s /sys/$$i ${DESTDIR}/usr/include/$$i; \
	done
	cd ${DESTDIR}/usr/include && rm -rf ${MACHINE} ${MACHINE_CPU} machine
	ln -s /sys/arch/${MACHINE}/include ${DESTDIR}/usr/include/${MACHINE}
	if test ${MACHINE} != ${MACHINE_CPU} -a \
	    -d ../sys/arch/${MACHINE_CPU}/include ; then \
		ln -s /sys/arch/${MACHINE_CPU}/include \
		    ${DESTDIR}/usr/include/${MACHINE_CPU} ; \
	fi
	ln -sf ${MACHINE} ${DESTDIR}/usr/include/machine

includes: ${SYS_INCLUDE}

.PHONY: prereq includes copies symlink
.include <bsd.prog.mk>
