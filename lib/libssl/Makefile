.include <bsd.own.mk>

ECHO= /bin/echo

.if exists(${.OBJDIR}/src-patent)
SUBDIR= crypto-patent ssl-patent ssleay 
.else
SUBDIR= crypto ssl ssleay
.endif

.include <bsd.subdir.mk>

