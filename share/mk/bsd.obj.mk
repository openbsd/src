#	$OpenBSD: bsd.obj.mk,v 1.7 1997/04/27 15:47:49 niklas Exp $
#	$NetBSD: bsd.obj.mk,v 1.9 1996/04/10 21:08:05 thorpej Exp $

.if !target(obj)
.if defined(NOOBJ)
obj:
.else

.if defined(MAKEOBJDIR)
__baseobjdir=	${MAKEOBJDIR}
.else
__baseobjdir=	obj
.endif

.if defined(OBJMACHINE)
__objdir=	${__baseobjdir}.${MACHINE}
.else
__objdir=	${__baseobjdir}
.endif

.if defined(USR_OBJMACHINE)
__usrobjdir=	${BSDOBJDIR}.${MACHINE}
__usrobjdirpf=	
.else
__usrobjdir=	${BSDOBJDIR}
.if defined(OBJMACHINE)
__usrobjdirpf=	.${MACHINE}
.else
__usrobjdirpf=
.endif
.endif

obj! _SUBDIRUSE
	@cd ${.CURDIR}; rm -f ${__objdir} > /dev/null 2>&1 || true; \
	here=`/bin/pwd`; bsdsrcdir=`cd ${BSDSRCDIR}; /bin/pwd`; \
	subdir=$${here#$${bsdsrcdir}/}; \
	if test $$here != $$subdir ; then \
		dest=${__usrobjdir}/$$subdir${__usrobjdirpf} ; \
		echo "$$here/${__objdir} -> $$dest"; \
		if test ! -L ${__objdir} -o \
		    X`perl -e "print readlink('${__objdir}')"` != X$$dest; \
		    then \
			test -e ${__objdir} && rm -rf ${__objdir}; \
			ln -s $$dest ${__objdir}; \
		fi; \
		if test -d ${__usrobjdir} -a ! -d $$dest; then \
			mkdir -p $$dest; \
		else \
			true; \
		fi; \
	else \
		true ; \
		dest=$$here/${__objdir} ; \
		if test ! -d ${__objdir} ; then \
			echo "making $$dest" ; \
			mkdir $$dest; \
		fi ; \
	fi;
.endif
.endif
