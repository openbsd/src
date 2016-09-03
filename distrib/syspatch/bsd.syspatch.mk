#	$OpenBSD: bsd.syspatch.mk,v 1.1 2016/09/03 21:43:25 robert Exp $
#
# Copyright (c) 2016 Robert Nagy <robert@openbsd.org>
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

# both the X.Y and XY formats are required
OSREV!=		uname -r
_OSREV=		${OSREV:S/.//g}

# binaries used by this makefile
FETCH=		/usr/bin/ftp -Vm
SIGNIFY=	/usr/bin/signify
PATCH=		/usr/bin/patch
MAKE=		/usr/bin/make
MTREE=		/usr/sbin/mtree

# make sure to only use the original OpenBSD mirror
MIRROR=		http://ftp.openbsd.org/pub/OpenBSD/patches/${OSREV}/common

# the final name of the syspatch tarball
SYSPATCH=	syspatch-${_OSREV}-${ERRATA}.tgz

# arguments used by different tools
MTREE_FILES=	/etc/mtree/4.4BSD.dist
MTREE_ARGS=	-qdep ${FAKE} -U
SIGNIFY_KEY=	/etc/signify/openbsd-${_OSREV}-base.pub
PATCH_STRIP?=	-p0
PATCH_ARGS=	-d ${SRCDIR} -z .orig --forward --quiet -E ${PATCH_STRIP}

# build type defaults to src
BUILD?=		src

# miscellaneous variables
SYSPATCH_DIR=	${FAKE}/var/syspatch/${OSREV}
FAKE=		${ERRATA}/fake
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
all: ${SYSPATCH}

.if !target(clean)
clean:
	rm -rf .depend ${ERRATA} ${SYSPATCH}
.endif

cleandir: clean

${_FAKE_COOKIE}:
	@${INSTALL} -d -m 755 ${SYSPATCH_DIR}
.for _m in ${MTREE_FILES}
	@${MTREE} ${MTREE_ARGS} -f ${_m} >/dev/null
.endfor
	@touch $@

${ERRATA}/${ERRATA}.patch: ${_FAKE_COOKIE}
	@echo ">> Fetch ${MIRROR}/${.TARGET:T}.sig"; \
	if ${FETCH} -o ${SYSPATCH_DIR}/${.TARGET:T}.sig \
		${MIRROR}/${.TARGET:T}.sig; then \
		if ${SIGNIFY} -Vep ${SIGNIFY_KEY} -x \
			${SYSPATCH_DIR}/${.TARGET:T}.sig -m ${.TARGET}; then \
				exit 0; \
		fi; \
	fi; exit 1

${_PATCH_COOKIE}: ${ERRATA}/${ERRATA}.patch
	@${PATCH} ${PATCH_ARGS} < ${ERRATA}/${ERRATA}.patch || \
		{ echo "***>   ${ERRATA}.patch did not apply cleanly"; \
		exit 1; };
	@touch $@

${_BUILD_COOKIE}: ${_PATCH_COOKIE}
.if ${BUILD:L:Msrc} || ${BUILD:L:Mxenocara}
.  if defined(SUBDIR) && !empty(SUBDIR)
.    for _s in ${SUBDIR}
	@if [ -f ${_s}/Makefile.bsd-wrapper ]; then \
		_mk_spec_="-f Makefile.bsd-wrapper"; \
	fi; \
	for _t in obj depend all; do \
		cd ${_s} && ${MAKE} $${_mk_spec_} $${_t}; \
	done; \
	cd ${_s} && ${MAKE} $${_mk_spec_} DESTDIR=${.OBJDIR}/${FAKE} install
.    endfor
.  endif
.elif ${BUILD:L:Mkernel}
.  for _kern in GENERIC GENERIC.MP
	@if cd ${SRCDIR}/sys/arch/${MACHINE_ARCH}/conf; then \
		if config ${_kern}; then \
			if cd ../compile/${_kern} && make clean && make; then \
				exit 0; \
			fi; exit 1; \
		fi; exit 1; \
	fi; exit 1
	@if [ ${_kern} = "GENERIC" ]; then \
		cp -p ${SRCDIR}/sys/arch/${MACHINE_ARCH}/compile/${_kern}/bsd \
			${.OBJDIR}/${FAKE}/bsd || \
			{ echo "***>   failed to install ${_kern}"; \
			exit 1; }; \
	elif [ ${_kern} = "GENERIC.MP" ]; then \
		cp -p ${SRCDIR}/sys/arch/${MACHINE_ARCH}/compile/${_kern}/bsd \
			${.OBJDIR}/${FAKE}/bsd.mp || \
			{ echo "***>   failed to install ${_kern}"; \
			exit 1; }; \
	fi; exit 0
.  endfor
.endif
	@touch $@

${SYSPATCH}: ${ERRATA}/.plist
.for _m in ${MTREE_FILES}
	@${MTREE} ${MTREE_ARGS} -f ${_m} >/dev/null
.endfor
	@tar -Pczf ${.TARGET} -C ${FAKE} -I ${ERRATA}/.plist || \
		{ echo "***>   unable to create ${.TARGET}"; \
		exit 1; };
	@echo ">> Created ${SYSPATCH}"; \

${ERRATA}/.fplist: ${_BUILD_COOKIE}
	@find ${FAKE} \! -type d > ${.OBJDIR}/${ERRATA}/.fplist || \
		{ echo "***>   unable to create list of files"; \
		exit 1; };

${ERRATA}/.plist: ${ERRATA}/.fplist
	@for _l in $$(cat ${.OBJDIR}/${ERRATA}/.fplist); do \
		_o=$$(echo $${_l} | sed "s,${FAKE},,g"); \
		cmp -s $${_l} $${_o} || echo $${_o} | sed 's,^/,,g'; \
	done > ${.OBJDIR}/${ERRATA}/.plist

findstatic:
.if defined(LIB) && !empty(LIB)
	@cd ${SRCDIR} && for _m in $$(find {bin,sbin} \
		\( -name Makefile -o -name Makefile.bsd-wrapper \) \
		-exec grep -l '\-l${LIB}' {} \;); do \
		echo "SUBDIR+= $$(dirname $${_m})"; \
	done
.endif

.include <bsd.obj.mk>
