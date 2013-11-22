#	$OpenBSD: bsd.obj.mk,v 1.14 2013/11/22 15:43:18 espie Exp $
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
		fi; \
		if test -d ${BSDOBJDIR}; then \
			test -d $$dest || mkdir -p $$dest; \
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
			mkdir $$dest; \
		fi ; \
	fi;
.  endif
.endif
