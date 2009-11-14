# $OpenBSD: Makefile,v 1.6 2009/11/14 10:13:32 espie Exp $

REGRESS_TARGETS=pkgnames depends-check longnames

PKG_ADD=perl ${.CURDIR}/myadd
PKG_CREATE=perl ${.CURDIR}/mycreate
LONG1=iamareallongfilenamethatcantberepresentedcorrectlyinatararchivebutwewantittohappenanywaysohmygodaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa42
LONG2=iamanotherreallygfilenamethatcantberepresentedcorrectlyinatararchivebutwewantittohappenanywaysohmygodaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa42
LONG3=iamanotherreallylonglinkthatcantberepresentedcorrectlyinatararchivebutwewantittohappenanywaysohmygodaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa42
LONG4=iamanotherreallylonghardlinkthatcantberepresentedcorrectlyinatararchivebutwewantittohappenanywaysohmygodaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa42

SRC1=${.OBJDIR}/src1/usr/local
DEST2=${.OBJDIR}/dest2/usr/local

pkgnames:
	perl ${.CURDIR}/check-name

depends-check: old/a-0.0.tgz old/b-0.0.tgz new/a-1.0.tgz new/b-1.0.tgz
	-rm -rf ${.OBJDIR}/dest1
	ROOT=${.OBJDIR}/dest1 ${PKG_ADD} old/a-0.0.tgz old/b-0.0.tgz
	ROOT=${.OBJDIR}/dest1 PKG_PATH=${.OBJDIR}/new ${PKG_ADD} -u a b
	ROOT=${.OBJDIR}/dest1 PKG_PATH=${.OBJDIR}/old ${PKG_ADD} -F downgrade -u a b

old/a-0.0.tgz:
	${PKG_CREATE} -P'test/b:b-*:b-0.0' -f ${.CURDIR}/empty $@

new/a-1.0.tgz:
	${PKG_CREATE} -f ${.CURDIR}/empty $@

old/b-0.0.tgz:
	${PKG_CREATE} -f ${.CURDIR}/empty $@

new/b-1.0.tgz:
	${PKG_CREATE} -P'test/a:a-*:a-1.0' -f ${.CURDIR}/empty $@


longnames: new/c-0.0.tgz
	-rm -rf ${.OBJDIR}/dest2
	ROOT=${.OBJDIR}/dest2 ${PKG_ADD} new/c-0.0.tgz
	@test -f ${DEST2}/${LONG1}
	@test -f ${DEST2}/${LONG2}
	@cd ${DEST2} && test -h ${LONG3} && test `readlink ${LONG3}` = ${LONG2}
	@cd ${DEST2} && test `stat -f '%i' ${LONG1}` = `stat -f '%i' ${LONG4}`

plist1:
	@echo "@owner "`whoami` >$@
	@echo ${LONG1} >>$@
	@echo ${LONG2} >>$@
	@echo ${LONG3} >>$@
	@echo ${LONG4} >>$@

new/c-0.0.tgz: plist1
	mkdir -p ${SRC1}
	@touch ${SRC1}/${LONG1}
	@touch ${SRC1}/${LONG2}
	@cd ${SRC1} && ln -sf ${LONG2} ${LONG3}
	@cd ${SRC1} && ln -f ${LONG1} ${LONG4}
	${PKG_CREATE} -B ${.OBJDIR}/src1 -f plist1 $@

.PHONY: pkgnames depends-check longnames

clean:
	-rm -rf old new dest* plist* src*

.include <bsd.regress.mk>
