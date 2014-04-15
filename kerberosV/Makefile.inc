# $OpenBSD: Makefile.inc,v 1.22 2014/04/15 20:55:42 miod Exp $

KRB5DIR=	${.CURDIR}/../../../kerberosV/src
KRB5SUBST=	${KRB5DIR}/../varsub

CPPFLAGS+=	-DHAVE_CONFIG_H -DNO_NTLM
CPPFLAGS+=	-I${.CURDIR}/../../include -I/usr/include/kerberosV

LIBHEIMIPCC=	${.OBJDIR}/../../lib/libheimipcc/libheimipcc.a
LIBHEIMIPCS=	${.OBJDIR}/../../lib/libheimipcs/libheimipcs.a
LIBVERS=	${.OBJDIR}/../../lib/libvers/libvers.a
LIBSL=		${.OBJDIR}/../../lib/libsl/libsl.a

.if defined(SRCS)

ETSRCS=	\
	${KRB5DIR}/lib/asn1/asn1_err.et \
	${KRB5DIR}/lib/hdb/hdb_err.et \
	${KRB5DIR}/lib/kadm5/kadm5_err.et \
	${KRB5DIR}/lib/krb5/heim_err.et \
	${KRB5DIR}/lib/krb5/k524_err.et \
	${KRB5DIR}/lib/krb5/krb5_err.et \
	${KRB5DIR}/lib/krb5/krb_err.et \
	${KRB5DIR}/lib/gssapi/krb5/gkrb5_err.et \
	${KRB5DIR}/lib/wind/wind_err.et \
	${KRB5DIR}/lib/ntlm/ntlm_err.et

.for ET in ${ETSRCS}
.  for _ET in ${ET:T:R}
.    if ${SRCS:M${_ET}.[ch]} != ""
.ORDER: ${_ET}.c ${_ET}.h
${_ET}.c ${_ET}.h: ${ET}
	${COMPILE_ET} ${.ALLSRC}
CLEANFILES+=	${_ET}.h ${_ET}.c
.    endif
.  endfor
.endfor

.endif # defined(SRCS)

.if defined(MPAGES)

MAN += ${MPAGES:S@^@${.OBJDIR}/@}
CLEANFILES += ${MAN}

.  for PAGE in ${MPAGES}
.    for dir in ${MSRC}
.      if exists(${dir}/${PAGE:T})
${PAGE}: ${dir}/${PAGE:T}
	sed -f ${KRB5SUBST} ${dir}/${PAGE:T} >$@.tmp && mv $@.tmp $@
.      endif
.    endfor
.  endfor

.endif # defined(MPAGES)

ASN1_COMPILE=	asn1_compile
SLC=		slc
COMPILE_ET=	compile_et


INCSGROUPS?=	INCS
INCLUDEDIR?=	/usr/include/kerberosV
INCSDIR?=	${INCLUDEDIR}

.if !target(includes)
.  for group in ${INCSGROUPS}
.    if defined(${group}) && !empty(${group})

${group}OWN?=	${BINOWN}
${group}GRP?=	${BINGRP}
${group}MODE?=	${NONBINMODE}
${group}DIR?=	/usr/include

_${group}INCS=
.      for header in ${${group}}
.        if defined(${group}OWN_${header:T}) || defined(${group}GRP_${header:T}) || \
            defined(${group}MODE_${header:T}) || defined(${group}DIR_${header:T}) || \
            defined(${group}NAME_${header:T})
${group}OWN_${header:T} ?=	${${group}OWN}
${group}GRP_${header:T} ?=	${${group}GRP}
${group}MODE_${header:T} ?=	${${group}MODE}
${group}DIR_${header:T} ?=	${${group}DIR}
.          if defined(${group}NAME)
${group}NAME_${header:T} ?=	${${group}NAME}
.          else
${group}NAME_${header:T} ?=	${header:T}
.          endif
includes: _${group}INS_${header:T}
_${group}INS_${header:T}: ${header}
	test -d ${DESTDIR}${${group}DIR_${.ALLSRC:T}}) || \
	    ${INSTALL} -d -o root -g bin -m 755 \
		${DESTDIR}${${group}DIR_${.ALLSRC:T}}
	${INSTALL} -C -o ${${group}OWN_${.ALLSRC:T}} \
	    -g ${${group}GRP_${.ALLSRC:T}} -m ${${group}MODE_${.ALLSRC:T}} \
	    ${.ALLSRC} \
	    ${DESTDIR}${${group}DIR_${.ALLSRC:T}}/${${group}NAME_${.ALLSRC:T}}
.        else
_${group}INCS+= ${header}
.        endif
.      endfor
.      if !empty(_${group}INCS)
includes: _${group}INS
_${group}INS: ${_${group}INCS}
	test -d ${DESTDIR}${${group}DIR} || \
	    ${INSTALL} -d -o root -g bin -m 755 \
		${DESTDIR}${${group}DIR}
.        if defined(${group}NAME)
	${INSTALL} -C -o ${${group}OWN} -g ${${group}GRP} -m ${${group}MODE} \
	    ${.ALLSRC} ${DESTDIR}${${group}DIR}/${${group}NAME}
.        else
	${INSTALL} -C -o ${${group}OWN} -g ${${group}GRP} -m ${${group}MODE} \
	    ${.ALLSRC} ${DESTDIR}${${group}DIR}/
.        endif
.      endif
.    endif # defined(${group}) && !empty(${group})
.  endfor
.endif

.include <bsd.own.mk>
