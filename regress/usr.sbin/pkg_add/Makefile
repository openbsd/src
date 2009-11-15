# $OpenBSD: Makefile,v 1.10 2009/11/15 08:38:19 espie Exp $

REGRESS_TARGETS=pkgnames depends-check longnames

PKG_ADD=perl ${.CURDIR}/myadd
PKG_CREATE=perl ${.CURDIR}/mycreate
LONG1=iamareallongfilenamethatcantberepresentedcorrectlyinatararchivebutwewantittohappenanywaysohmygodaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa42
LONG2=iamanotherreallygfilenamethatcantberepresentedcorrectlyinatararchivebutwewantittohappenanywaysohmygodaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa42
LONG3=iamanotherreallylonglinkthatcantberepresentedcorrectlyinatararchivebutwewantittohappenanywaysohmygodaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa42
LONG4=iamanotherreallylonghardlinkthatcantberepresentedcorrectlyinatararchivebutwewantittohappenanywaysohmygodaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa42

.for i in 1 2 3 4 5 6 7 8 9
S$i ?= ${.OBJDIR}/src$i
SRC$i ?= ${S$i}/usr/local
D$i ?= ${.OBJDIR}/dest$i
DEST$i ?= ${D$i}/usr/local
.endfor

pkgnames:
	perl ${.CURDIR}/check-name

depends-check: old/a-0.0.tgz old/b-0.0.tgz new/a-1.0.tgz new/b-1.0.tgz
	-rm -rf ${D1}
	ROOT=${D1} ${PKG_ADD} old/a-0.0.tgz old/b-0.0.tgz
	ROOT=${D1} PKG_PATH=${.OBJDIR}/new ${PKG_ADD} -u a b
	ROOT=${D1} PKG_PATH=${.OBJDIR}/old ${PKG_ADD} -F downgrade -u a b

longnames: new/c-0.0.tgz
	-rm -rf ${D2}
	ROOT=${D2} ${PKG_ADD} new/c-0.0.tgz
	@test -f ${DEST2}/${LONG1}
	@test -f ${DEST2}/${LONG2}
	@cd ${DEST2} && test -h ${LONG3} && test `readlink ${LONG3}` = ${LONG2}
	@cd ${DEST2} && test `stat -f '%i' ${LONG1}` = `stat -f '%i' ${LONG4}`

collision-check1: new/d-0.0.tgz new/e-0.0.tgz
	-rm -rf ${D3}
	ROOT=${D3} ${PKG_ADD} new/d-0.0.tgz
	ROOT=${D3} ${PKG_ADD} new/e-0.0.tgz

collision-check2: new/d-0.0.tgz new/e-0.0.tgz
	-rm -rf ${D4}
	ROOT=${D4} ${PKG_ADD} new/d-0.0.tgz
	-rm -rf ${D4}/pkgdb/d-0.0
	ROOT=${D4} ${PKG_ADD} new/e-0.0.tgz

collision-check3: new/d-0.0.tgz new/f-0.0.tgz
	-rm -rf ${D5}
	ROOT=${D5} ${PKG_ADD} new/d-0.0.tgz new/f-0.0.tgz
	-rm -rf ${D5}/pkgdb/d-0.0
	ROOT=${D5} ${PKG_ADD} -F repair new/d-0.0.tgz
	PKG_DBDIR=${D5}/pkgdb pkg_info -qR d

collision-check4: new/d-0.0.tgz new/e-0.0.tgz
	-rm -rf ${D6}
	ROOT=${D6} ${PKG_ADD} new/d-0.0.tgz
	-rm -rf ${D6}/pkgdb/d-0.0
	ROOT=${D6} ${PKG_ADD} -F repair new/e-0.0.tgz

collision-check5: new/d-0.0.tgz new/e-0.0.tgz
	-rm -rf ${D7}
	ROOT=${D7} ${PKG_ADD} new/d-0.0.tgz
	-rm -rf ${D7}/pkgdb/d-0.0
	ROOT=${D7} ${PKG_ADD} -F removecollisions new/d-0.0.tgz

old/a-0.0.tgz:
	${PKG_CREATE} -P'test/b:b-*:b-0.0' -f ${.CURDIR}/empty $@

new/a-1.0.tgz:
	${PKG_CREATE} -f ${.CURDIR}/empty $@

old/b-0.0.tgz:
	${PKG_CREATE} -f ${.CURDIR}/empty $@

new/b-1.0.tgz:
	${PKG_CREATE} -P'test/a:a-*:a-1.0' -f ${.CURDIR}/empty $@



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

new/f-0.0.tgz:
	${PKG_CREATE} -P'test/d:d-*:d-0.0' -f ${.CURDIR}/empty $@

.PHONY: pkgnames depends-check longnames \
	collision-check1 collision-check2 collision-check3 \
	collision-check4 collision-check5

clean:
	-rm -rf old new dest* plist* src*

.include <bsd.regress.mk>
