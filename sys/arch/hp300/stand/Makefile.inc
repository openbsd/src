#	$OpenBSD: Makefile.inc,v 1.4 2002/12/02 09:00:23 miod Exp $
#	$NetBSD: Makefile.inc,v 1.7 1997/05/12 07:43:18 thorpej Exp $

BINDIR=			/usr/mdec

# RELOC=FFF00000 allows for boot prog up to FF000 (1044480) bytes long
RELOC=	FFF00000

CFLAGS+=		-fno-stack-protector

CPPFLAGS+=		-DDCACONSOLE -DAPCICONSOLE -DITECONSOLE -DDCMCONSOLE
CPPFLAGS+=		-DHIL_KEYBOARD -DUK_KEYBOARD
CPPFLAGS+=		-DDOMAIN_KEYBOARD
CPPFLAGS+=		-DSTANDALONE -Dhp300
CPPFLAGS+=		-DCOMPAT_UFS -DNO_LSEEK

# Limit the alloc() heap to below the msgbuf and ROM scratch pages.
CPPFLAGS+=		-DHEAP_LIMIT="0xffffdfff"

### Figure out what to use for libcommon
COMMONDIR=		${.CURDIR}/../common

.if exists(${COMMONDIR}/obj.${MACHINE})
COMMONOBJDIR=		${COMMONDIR}/obj.${MACHINE}
.elif exists(${COMMONDIR}/obj)
COMMONOBJDIR=		${COMMONDIR}/obj
.else
COMMONOBJDIR=		${COMMONDIR}
.endif

LIBCOMMON=		${COMMONOBJDIR}/libcommon.a

### Figure out what to use for libkern
LIBKERNDIR=		${.CURDIR}/../libkern

.if exists(${LIBKERNDIR}/obj.${MACHINE})
LIBKERNOBJDIR=		${LIBKERNDIR}/obj.${MACHINE}
.elif exists(${LIBKERNDIR}/obj)
LIBKERNOBJDIR=		${LIBKERNDIR}/obj
.else
LIBKERNOBJDIR=		${LIBKERNDIR}
.endif

LIBKERN=		${LIBKERNOBJDIR}/libkern.a

### Figure out what to use for libsa
LIBSADIR=		${.CURDIR}/../libsa

.if exists(${LIBSADIR}/obj.${MACHINE})
LIBSAOBJDIR=		${LIBSADIR}/obj.${MACHINE}
.elif exists(${LIBSADIR}/obj)
LIBSAOBJDIR=		${LIBSADIR}/obj
.else
LIBSAOBJDIR=		${LIBSADIR}
.endif

LIBSA=			${LIBSAOBJDIR}/libsa.a

### Figure out what to use for libsaz
LIBSAZDIR=		${.CURDIR}/../libsaz

.if exists(${LIBSAZDIR}/obj.${MACHINE})
LIBSAZOBJDIR=		${LIBSAZDIR}/obj.${MACHINE}
.elif exists(${LIBSAZDIR}/obj)
LIBSAZOBJDIR=		${LIBSAZDIR}/obj
.else
LIBSAZOBJDIR=		${LIBSAZDIR}
.endif

LIBSAZ=			${LIBSAZOBJDIR}/libsaz.a

### Figure out what to use for libz
LIBZDIR=		${.CURDIR}/../libz

.if exists(${LIBZDIR}/obj.${MACHINE})
LIBZOBJDIR=		${LIBZDIR}/obj.${MACHINE}
.elif exists(${LIBZDIR}/obj)
LIBZOBJDIR=		${LIBZDIR}/obj
.else
LIBZOBJDIR=		${LIBZDIR}
.endif

LIBZ=			${LIBZOBJDIR}/libz.a

### Figure out what to use for mkboot
MKBOOTDIR=		${.CURDIR}/../mkboot

.if exists(${MKBOOTDIR}/obj.${MACHINE})
MKBOOTOBJDIR=		${MKBOOTDIR}/obj.${MACHINE}
.elif exists(${MKBOOTDIR}/obj)
MKBOOTOBJDIR=		${MKBOOTDIR}/obj
.else
MKBOOTOBJDIR=		${MKBOOTDIR}
.endif

MKBOOT_PROG=		${MKBOOTOBJDIR}/mkboot

### Figure out what to use for stripboot
STRIPBOOTDIR=		${.CURDIR}/../stripboot

.if exists(${STRIPBOOTDIR}/obj.${MACHINE})
STRIPBOOTOBJDIR=	${STRIPBOOTDIR}/obj.${MACHINE}
.elif exists(${STRIPBOOTDIR}/obj)
STRIPBOOTOBJDIR=	${STRIPBOOTDIR}/obj
.else
STRIPBOOTOBJDIR=	${STRIPBOOTDIR}
.endif

STRIPBOOT_PROG=		${STRIPBOOTOBJDIR}/stripboot

# Make sure ${MKBOOT_PROG} is always available
${MKBOOT_PROG}:
	@cd ${MKBOOTDIR} && make depend && make

# Make sure ${STRIPBOOT_PROG} is always available
${STRIPBOOT_PROG}:
	@cd ${STRIPBOOTDIR} && make depend && make
