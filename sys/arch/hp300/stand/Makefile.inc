#	$OpenBSD: Makefile.inc,v 1.11 2011/08/18 20:02:58 miod Exp $
#	$NetBSD: Makefile.inc,v 1.7 1997/05/12 07:43:18 thorpej Exp $

BINDIR=			/usr/mdec

# RELOC=FFF00000 allows for boot prog up to FF000 (1044480) bytes long
RELOC=	FFF00000

CFLAGS+=		-Wall -Wstrict-prototypes -Wmissing-prototypes \
			-fno-stack-protector -msoft-float

CPPFLAGS+=		-DAPCICONSOLE -DDCACONSOLE -DDCMCONSOLE -DITECONSOLE
CPPFLAGS+=		-DHIL_KEYBOARD -DDOMAIN_KEYBOARD
CPPFLAGS+=		-DSTANDALONE -D__hp300__
CPPFLAGS+=		-DCOMPAT_UFS -DNO_LSEEK

# Limit the alloc() heap to below the msgbuf and ROM scratch pages.
CPPFLAGS+=		-DHEAP_LIMIT="0xffffdfff"

### Figure out what to use for libcommon
COMMONDIR=		${.CURDIR}/../common

.if exists(${COMMONDIR}/${__objdir})
COMMONOBJDIR=		${COMMONDIR}/${__objdir}
.else
COMMONOBJDIR=		${COMMONDIR}
.endif

LIBCOMMON=		${COMMONOBJDIR}/libcommon.a

### Figure out what to use for libsa
LIBSADIR=		${.CURDIR}/../libsa

.if exists(${LIBSADIR}/${__objdir})
LIBSAOBJDIR=		${LIBSADIR}/${__objdir}
.else
LIBSAOBJDIR=		${LIBSADIR}
.endif

LIBSA=			${LIBSAOBJDIR}/libsa.a

### Figure out what to use for libsaz
LIBSAZDIR=		${.CURDIR}/../libsaz

.if exists(${LIBSAZDIR}/${__objdir})
LIBSAZOBJDIR=		${LIBSAZDIR}/${__objdir}
.else
LIBSAZOBJDIR=		${LIBSAZDIR}
.endif

LIBSAZ=			${LIBSAZOBJDIR}/libsaz.a

### Figure out what to use for libz
LIBZDIR=		${.CURDIR}/../libz

.if exists(${LIBZDIR}/${__objdir})
LIBZOBJDIR=		${LIBZDIR}/${__objdir}
.else
LIBZOBJDIR=		${LIBZDIR}
.endif

LIBZ=			${LIBZOBJDIR}/libz.a

### Figure out what to use for mkboot
MKBOOTDIR=		${.CURDIR}/../mkboot

.if exists(${MKBOOTDIR}/${__objdir})
MKBOOTOBJDIR=		${MKBOOTDIR}/${__objdir}
.else
MKBOOTOBJDIR=		${MKBOOTDIR}
.endif

MKBOOT_PROG=		${MKBOOTOBJDIR}/mkboot

### Figure out what to use for stripboot
STRIPBOOTDIR=		${.CURDIR}/../stripboot

.if exists(${STRIPBOOTDIR}/${__objdir})
STRIPBOOTOBJDIR=	${STRIPBOOTDIR}/${__objdir}
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
