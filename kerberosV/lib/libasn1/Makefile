# $OpenBSD: Makefile,v 1.3 2013/06/21 10:59:40 ajacoutot Exp $

LIB=	asn1
INCS=	asn1_err.h asn1-common.h heim_asn1.h der.h der-protos.h der-private.h

SRCS=	asn1_err.c \
	asn1_err.h \
	der.c \
	der_copy.c \
	der_cmp.c \
	der_free.c \
	der_format.c \
	der_get.c \
	der_length.c \
	der_put.c \
	extra.c \
	template.c \
	timegm.c \
	roken.h \
	${GEN:S/.x$/.c/:S/.hx$/.h/}

CFLAGS+=-I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/roken -I${KRB5DIR}/lib/com_err -I.

GEN_RFC2459=	asn1_rfc2459_asn1.x rfc2459_asn1.hx rfc2459_asn1-priv.hx
GEN_CMS= 	asn1_cms_asn1.x cms_asn1.hx cms_asn1-priv.hx
GEN_K5=		asn1_krb5_asn1.x krb5_asn1.hx krb5_asn1-priv.hx

GEN+=	${GEN_RFC2459}
GEN+=	${GEN_CMS}
GEN+=	${GEN_K5}

CLEANFILES= ${GEN} ${GEN:S/.x$/.c/:S/.hx$/.h/} *_asn1_files

INCS+=	krb5_asn1.h \
	cms_asn1.h \
	rfc2459_asn1.h

${GEN_CMS}: cms.asn1 cms.opt
	${ASN1_COMPILE} --one-code-file \
		--option-file=${.ALLSRC:M*.opt} ${.ALLSRC:M*.asn1} cms_asn1

${GEN_RFC2459}: rfc2459.asn1
	${ASN1_COMPILE} \
		--one-code-file \
		--preserve-binary=TBSCertificate \
		--preserve-binary=TBSCRLCertList \
		--preserve-binary=Name \
		--sequence=GeneralNames \
		--sequence=Extensions \
		--sequence=CRLDistributionPoints ${.ALLSRC:M*.asn1} rfc2459_asn1

${GEN_K5}: krb5.asn1 krb5.opt
	${ASN1_COMPILE} \
		--one-code-file \
		--option-file=${.ALLSRC:M*.opt} \
		${.ALLSRC:M*.asn1} krb5_asn1

.SUFFIXES: .h .c .x .hx

.x.c:
	cmp -s ${.IMPSRC} ${.TARGET} 2> /dev/null || cp ${.IMPSRC} ${.TARGET}

.hx.h:
	cmp -s ${.IMPSRC} ${.TARGET} 2> /dev/null || cp ${.IMPSRC} ${.TARGET}

.include <bsd.lib.mk>

.SUFFIXES: .h .c .x .hx

.PATH: ${KRB5DIR}/lib/asn1 ${KRB5DIR}/include ${.CURDIR}/../../include
