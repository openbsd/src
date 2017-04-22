#	$OpenBSD: bsd.syspatch.mk,v 1.8 2017/04/22 13:39:00 robert Exp $
#
# Copyright (c) 2016-2017 Robert Nagy <robert@openbsd.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

.include <bsd.own.mk>

ERRATA?=
ECURR=${ERRATA:C/_.*//}
EPREV!=echo ${ECURR} | awk '{printf "%03d\n", $$1 - 1;}'

# binaries used by this makefile
FETCH=		/usr/bin/ftp -Vm

# make sure to only use the original OpenBSD mirror
MIRROR=		https://ftp.openbsd.org/pub/OpenBSD/patches/${OSREV}/common

# build type defaults to src
BUILD?=		src

SYSPATCH_BASE=	syspatch${OSrev}-${ERRATA}
SYSPATCH_SHRT=	${OSrev}-${ERRATA}

# the final name of the syspatch tarball
SYSPATCH=	${SYSPATCH_BASE}.tgz

# arguments used by different tools
MTREE_FILES=	/etc/mtree/4.4BSD.dist
MTREE_ARGS=	-qdep ${FAKE} -U
SIGNIFY_KEY=	/etc/signify/openbsd-${OSrev}-base.pub
PATCH_STRIP?=	-p0
PATCH_ARGS=	-d ${SRCDIR} -z .orig --forward --quiet -E ${PATCH_STRIP}

# miscellaneous variables
SYSPATCH_DIR=	${FAKE}/var/syspatch/${SYSPATCH_SHRT}
FAKE=		${FAKEROOT}/syspatch/${SYSPATCH_SHRT}
SRCDIR=		${BSDSRCDIR}
SUBDIR?=

_PATCH_COOKIE=	${ERRATA}/.patch_done
_BUILD_COOKIE=	${ERRATA}/.build_done
_FAKE_COOKIE=	${ERRATA}/.fake_done

.if ${BUILD:L:Msrc}
SRCDIR=		${BSDSRCDIR}
.elif ${BUILD:L:Mxenocara}
SRCDIR=		${X11SRC}
MTREE_FILES+=	/etc/mtree/BSD.x11.dist
.endif

.MAIN: all
all: ${_BUILD_COOKIE}

.if !target(clean)
clean:
	rm -rf .depend ${ERRATA} ${SYSPATCH}
.endif

depend:

cleandir: clean

${_FAKE_COOKIE}:
.ifndef FAKEROOT
	@{ echo "***>   setenv FAKEROOT before doing that!"; \
	exit 1; };
.else
	@if [[ `id -u` -ne 0 ]]; then \
		{ echo "***>   $@ must be called by root"; \
		exit 1; }; \
	fi
	@destmp=`df -P ${FAKEROOT} | awk 'END { print $$6 }'`; \
	if ! mount | grep -q " $${destmp} .*noperm"; then \
		echo ${FAKEROOT} must be on a noperm filesystem >&2; \
		false; \
	fi; \
	if [[ `stat -f '%Su %Lp' $${destmp}` != '${BUILDUSER} 700' ]]; then \
		echo $${destmp} must have owner BUILDUSER and mode 700 >&2; \
		false; \
	fi
	@test -d ${FAKEROOT}/syspatch/${OSrev}-${EPREV}_* || \
		{ echo "***>   previous (${EPREV}) syspatch build is missing"; \
		exit 1; }; \
	echo '>> Copying previous syspatch fakeroot to ${FAKE}'; \
	${INSTALL} -d -m 755 ${SYSPATCH_DIR}; \
	cd ${FAKEROOT}/syspatch/${OSrev}-${EPREV}_* && tar cf - . | \
		(cd ${FAKE} && tar xpf - )
	@${INSTALL} ${INSTALL_COPY} -o ${SHAREOWN} -g ${SHAREGRP} -m ${SHAREMODE} \
		${ERRATA}/${ERRATA}.patch.sig ${SYSPATCH_DIR}

.for _m in ${MTREE_FILES}
	@su ${BUILDUSER} -c '/usr/sbin/mtree ${MTREE_ARGS} -f ${_m}' >/dev/null
.endfor
	@su ${BUILDUSER} -c 'touch $@'
.endif

${ERRATA}/${ERRATA}.patch:
	@su ${BUILDUSER} -c '${INSTALL} -d -m 755 ${ERRATA}' && \
	echo '>> Fetching & Verifying ${MIRROR}/${.TARGET:T}.sig'; \
	if su ${BUILDUSER} -c '${FETCH} -o ${ERRATA}/${.TARGET:T}.sig \
		${MIRROR}/${.TARGET:T}.sig'; then \
		su ${BUILDUSER} -c '/usr/bin/signify -Vep ${SIGNIFY_KEY} -x \
			${ERRATA}/${.TARGET:T}.sig -m ${.TARGET}' && exit 0; \
	fi; exit 1

${_PATCH_COOKIE}: ${ERRATA}/${ERRATA}.patch
	@echo '>> Applying ${ERRATA}.patch'; \
	su ${BUILDUSER} -c '/usr/bin/patch ${PATCH_ARGS} < ${ERRATA}/${ERRATA}.patch' || \
		{ echo "***>   ${ERRATA}.patch did not apply cleanly"; \
		exit 1; };
	@su ${BUILDUSER} -c 'touch $@'

.ifdef DESTDIR
${_BUILD_COOKIE}: ${_PATCH_COOKIE} ${_FAKE_COOKIE}
	@{ echo "***>   cannot set DESTDIR here!"; \
	exit 1; };
.elif !defined(FAKEROOT)
${_BUILD_COOKIE}: ${_PATCH_COOKIE} ${_FAKE_COOKIE}
	@{ echo "***>   setenv FAKEROOT before doing that!"; \
	exit 1; };
.else
${_BUILD_COOKIE}: ${_PATCH_COOKIE} ${_FAKE_COOKIE}
	@echo '>> Building syspatch for ${ERRATA}'
.if ${BUILD:L:Msrc}
. for _t in clean obj build
	@su ${BUILDUSER} -c "cd ${SRCDIR} && /usr/bin/make SYSPATCH=Yes ${_t}"
. endfor
	@su ${BUILDUSER} -c "cd ${SRCDIR} && make SYSPATCH=Yes DESTDIR=${FAKE} install"
.elif ${BUILD:L:Mxenocara}
. for _t in clean bootstrap obj build
	@su ${BUILDUSER} -c "cd ${SRCDIR} && /usr/bin/make SYSPATCH=Yes ${_t}"
. endfor
	@su ${BUILDUSER} -c "cd ${SRCDIR} && make SYSPATCH=Yes DESTDIR=${FAKE} install"
.elif ${BUILD:L:Mkernel}
.  for _kern in GENERIC GENERIC.MP
	@if cd ${SRCDIR}/sys/arch/${MACHINE_ARCH}/conf; then \
		if config ${_kern}; then \
			if cd ../compile/${_kern} && make; then \
				exit 0; \
			fi; exit 1; \
		fi; exit 1; \
	fi;
	@if [ ${_kern} = "GENERIC" ]; then \
		su ${BUILDUSER} -c '${INSTALL} ${INSTALL_COPY} -o ${SHAREOWN} -g ${LOCALEGRP} \
		-m 0644 ${SRCDIR}/sys/arch/${MACHINE_ARCH}/compile/${_kern}/bsd \
		${FAKE}/bsd' || \
		{ echo "***>   failed to install ${_kern}"; \
		exit 1; }; \
	elif [ ${_kern} = "GENERIC.MP" ]; then \
		su ${BUILDUSER} -c '${INSTALL} ${INSTALL_COPY} -o ${SHAREOWN} -g ${LOCALEGRP} \
		-m 0644 ${SRCDIR}/sys/arch/${MACHINE_ARCH}/compile/${_kern}/bsd \
		${FAKE}/bsd.mp' || \
		{ echo "***>   failed to install ${_kern}"; \
		exit 1; }; \
	fi; exit 0
.  endfor
.endif
	@su ${BUILDUSER} -c 'touch $@'
.endif

syspatch: ${SYSPATCH}

${SYSPATCH}: ${ERRATA}/.plist
.for _m in ${MTREE_FILES}
	@su ${BUILDUSER} -c '/usr/sbin/mtree ${MTREE_ARGS} -f ${_m}' >/dev/null
.endfor
	@su ${BUILDUSER} -c 'tar -Pczf ${.TARGET} -C ${FAKE} -I ${ERRATA}/.plist' || \
		{ echo "***>   unable to create ${.TARGET}"; \
		exit 1; };
	@echo ">> Created ${SYSPATCH}";

${ERRATA}/.plist: ${_BUILD_COOKIE}
	@echo ">> Creating the list of files to be included in ${SYSPATCH}"
	@su ${BUILDUSER} -c '							\
	${.CURDIR}/diff.sh ${FAKEROOT}/syspatch/${OSrev}-${EPREV}_* ${FAKE} 	\
		done > ${.TARGET}' || \
		{ echo "***>   unable to create list of files";	\
		exit 1; };
	@su ${BUILDUSER} -c 'echo ${SYSPATCH_DIR}/${ERRATA}.patch.sig >> ${.OBJDIR}/${ERRATA}/.plist' || \
		{ echo "***>   unable to add syspatch to list of files"; \
		exit 1; };
	@su ${BUILDUSER} -c 'sed -i "s,^${FAKEROOT}/syspatch/${OSrev}-[^/]*/,./,g" ${.TARGET}' 

.include <bsd.obj.mk>
