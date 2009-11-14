# $OpenBSD: Makefile,v 1.7 2009/11/14 10:25:53 espie Exp $

REGRESS_TARGETS=pkgnames depends-check longnames

PKG_ADD=perl ${.CURDIR}/myadd
PKG_CREATE=perl ${.CURDIR}/mycreate
LONG1=iamareallongfilenamethatcantberepresentedcorrectlyinatararchivebutwewantittohappenanywaysohmygodaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa42
LONG2=iamanotherreallygfilenamethatcantberepresentedcorrectlyinatararchivebutwewantittohappenanywaysohmygodaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa42
LONG3=iamanotherreallylonglinkthatcantberepresentedcorrectlyinatararchivebutwewantittohappenanywaysohmygodaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa42
LONG4=iamanotherreallylonghardlinkthatcantberepresentedcorrectlyinatararchivebutwewantittohappenanywaysohmygodaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa42

.for i in 1 2 3 4 5 6 7 8 9
SRC$i ?= ${.OBJDIR}/src$i/usr/local
DEST$i ?= ${.OBJDIR}/dest$i/usr/local
.endfor

pkgnames:
	perl ${.CURDIR}/check-name

depends-check: old/a-0.0.tgz old/b-0.0.tgz new/a-1.0.tgz new/b-1.0.tgz
	-rm -rf ${.OBJDIR}/dest1
	ROOT=${.OBJDIR}/dest1 ${PKG_ADD} old/a-0.0.tgz old/b-0.0.tgz
	ROOT=${.OBJDIR}/dest1 PKG_PATH=${.OBJDIR}/new ${PKG_ADD} -u a b
	ROOT=${.OBJDIR}/dest1 PKG_PATH=${.OBJDIR}/old ${PKG_ADD} -F downgrade -u a b

collision-check: new/d-0.0.tgz new/e-0.0.tgz
	ROOT=${.OBJDIR}/dest3 ${PKG_ADD} new/d-0.0.tgz
	ROOT=${.OBJDIR}/dest3 ${PKG_ADD} new/e-0.0.tgz

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

plist2:
	@echo "@owner "`whoami` >$@
	@echo a >>$@
	@echo b >>$@
	@echo c >>$@
	@echo f >>$@
	@echo g >>$@

plist3:
	@echo "@owner "`whoami` >$@
	@echo a >>$@
	@echo c >>$@
	@echo d >>$@
	@echo f >>$@
	@echo g >>$@

new/c-0.0.tgz: plist1
	mkdir -p ${SRC1}
	@touch ${SRC1}/${LONG1}
	@touch ${SRC1}/${LONG2}
	@cd ${SRC1} && ln -sf ${LONG2} ${LONG3}
	@cd ${SRC1} && ln -f ${LONG1} ${LONG4}
	${PKG_CREATE} -B ${.OBJDIR}/src1 -f plist1 $@

new/d-0.0.tgz: plist2
	mkdir -p ${SRC2}
	touch ${SRC2}/a ${SRC2}/b ${SRC2}/c
	echo "coucou" >${SRC2}/f
	echo "not coucou" >${SRC2}/g
	${PKG_CREATE} -B ${.OBJDIR}/src2 -f ${.OBJDIR}/plist2 $@

new/e-0.0.tgz: plist3
	mkdir -p ${SRC3}
	touch ${SRC3}/a ${SRC3}/c ${SRC3}/d
	echo "coucou" >${SRC3}/f
	echo "coucou" >${SRC3}/g
	${PKG_CREATE} -B ${.OBJDIR}/src3 -f ${.OBJDIR}/plist3 $@

.PHONY: pkgnames depends-check longnames

clean:
	-rm -rf old new dest* plist* src*

.include <bsd.regress.mk>
