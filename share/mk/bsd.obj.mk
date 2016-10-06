#	$OpenBSD: bsd.obj.mk,v 1.15 2016/10/06 15:34:18 natano Exp $
#	$NetBSD: bsd.obj.mk,v 1.9 1996/04/10 21:08:05 thorpej Exp $

.if !target(obj)
.  if defined(NOOBJ)
obj:
.  else

.  if defined(MAKEOBJDIR)
__objdir=	${MAKEOBJDIR}
.  else
__objdir=	obj
.  endif

_SUBDIRUSE:

obj! _SUBDIRUSE
	@cd ${.CURDIR}; \
	if [[ `id -u` -eq 0 && ${BUILDUSER} != root ]]; then \
		SETOWNER="chown -h ${BUILDUSER}"; \
		_mkdirs() { \
			su ${BUILDUSER} -c "mkdir -p $$1"; \
		}; \
		MKDIRS=_mkdirs; \
	else \
		MKDIRS="mkdir -p"; \
		SETOWNER=:; \
	fi; \
	here=`/bin/pwd`; bsdsrcdir=`cd ${BSDSRCDIR}; /bin/pwd`; \
	subdir=$${here#$${bsdsrcdir}/}; \
	if test $$here != $$subdir ; then \
		dest=${BSDOBJDIR}/$$subdir ; \
		echo "$$here/${__objdir} -> $$dest"; \
		if test ! -L ${__objdir} -o \
		    X`readlink ${__objdir}` != X$$dest; \
		    then \
			if test -e ${__objdir}; then rm -rf ${__objdir}; fi; \
			ln -sf $$dest ${__objdir}; \
			$$SETOWNER ${__objdir}; \
		fi; \
		if test -d ${BSDOBJDIR}; then \
			test -d $$dest || $$MKDIRS $$dest; \
		else \
			if test -e ${BSDOBJDIR}; then \
				echo "${BSDOBJDIR} is not a directory"; \
			else \
				echo "${BSDOBJDIR} does not exist"; \
			fi; \
		fi; \
	else \
		true ; \
		dest=$$here/${__objdir} ; \
		if test ! -d ${__objdir} ; then \
			echo "making $$dest" ; \
			$$MKDIRS $$dest; \
		fi ; \
	fi;
.  endif
.endif

.include <bsd.own.mk>
