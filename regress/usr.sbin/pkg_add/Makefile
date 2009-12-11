# $OpenBSD: Makefile,v 1.20 2009/12/11 13:02:59 espie Exp $

REGRESS_TARGETS=pkgnames depends-check longnames always-update \
	collision-check3 partial-update-test conflict-update \
	merge-update split-update

PKG_ADD=perl ${.CURDIR}/myadd
PKG_CREATE=perl ${.CURDIR}/mycreate
LONG1=iamareallongfilenamethatcantberepresentedcorrectlyinatararchivebutwewantittohappenanywaysohmygodaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa42
LONG2=iamanotherreallygfilenamethatcantberepresentedcorrectlyinatararchivebutwewantittohappenanywaysohmygodaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa42
LONG3=iamanotherreallylonglinkthatcantberepresentedcorrectlyinatararchivebutwewantittohappenanywaysohmygodaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa42
LONG4=iamanotherreallylonghardlinkthatcantberepresentedcorrectlyinatararchivebutwewantittohappenanywaysohmygodaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa42

.for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20
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

always-update: old/g-0.0.tgz new/g-0.0.tgz
	-rm -rf ${D8}
	ROOT=${D8} ${PKG_ADD} old/g-0.0.tgz
	ROOT=${D8} PKG_PATH=${.OBJDIR}/new ${PKG_ADD} -u
	PKG_DBDIR=${D8}/pkgdb pkg_info -qf g|fgrep -q comment2

list-check: new/a-1.0.tgz new/b-1.0.tgz new/c-0.0.tgz new/d-0.0.tgz \
	new/e-0.0.tgz new/f-0.0.tgz new/g-0.0.tgz
	-rm -rf ${D9}
	ROOT=${D9} PKG_PATH=${.OBJDIR}/new ${PKG_ADD} -l ${.CURDIR}/list
	PKG_DBDIR=${D9}/pkgdb pkg_info -q|diff - ${.CURDIR}/list.out

partial-update-test: old/h-0.0.tgz old/i-0.0.tgz old/j-0.0.tgz \
	new/h-1.0.tgz new/i-1.0.tgz new/j-1.0.tgz
	-rm -rf ${D10}
	ROOT=${D10} PKG_PATH=${.OBJDIR}/old ${PKG_ADD} h j
	ROOT=${D10} PKG_PATH=${.OBJDIR}/new ${PKG_ADD} -u h
	PKG_DBDIR=${D10}/pkgdb pkg_info -q|diff - ${.CURDIR}/list2.out

conflict-update: old/k-0.0.tgz old/l-0.0.tgz new/k-1.0.tgz new/l-1.0.tgz
	-rm -rf ${D11}
	ROOT=${D11} PKG_PATH=${.OBJDIR}/old ${PKG_ADD} k l
	ROOT=${D11} PKG_PATH=${.OBJDIR}/new ${PKG_ADD} -u k
	PKG_DBDIR=${D11}/pkgdb pkg_info -q|diff - ${.CURDIR}/list3.out

merge-update: old/m-0.0.tgz old/n-0.0.tgz new/m-1.0.tgz new/n-1.0.tgz
	-rm -rf ${D12}
	ROOT=${D12} PKG_PATH=${.OBJDIR}/old ${PKG_ADD} m n
	ROOT=${D12} PKG_PATH=${.OBJDIR}/new ${PKG_ADD} -u n
	PKG_DBDIR=${D12}/pkgdb pkg_info -q|diff - ${.CURDIR}/list4.out

split-update: old/o-0.0.tgz new/o-1.0.tgz new/p-0.0.tgz
	-rm -rf ${D13}
	ROOT=${D13} PKG_PATH=${.OBJDIR}/old ${PKG_ADD} o
	ROOT=${D13} PKG_PATH=${.OBJDIR}/new ${PKG_ADD} -u o
	PKG_DBDIR=${D13}/pkgdb pkg_info -q |diff - ${.CURDIR}/list5.out

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

plist4:
	@echo "@option always-update" >$@
	@echo "@comment comment1" >>$@

plist5:
	@echo "@option always-update" >$@
	@echo "@comment comment2" >>$@

plist6:
	@echo "@conflict l-<1.0" >$@

plist7:
	@echo "@conflict m-<1.0" >$@
	@echo "@pkgpath test/m" >>$@

plist8:
	@mkdir -p ${@D}
	@echo "@conflict o-<1.0" >$@
	@echo "@pkgpath test/o" >>$@
	@echo "@owner "`whoami` >>$@
	@echo '@lib lib/libcoincoin.so.$${LIBcoincoin_VERSION}' >>$@

old/a-0.0.tgz:
	${PKG_CREATE} -P'test/b:b-*:b-0.0' -f ${.CURDIR}/empty $@

new/a-1.0.tgz old/b-0.0.tgz old/i-0.0.tgz old/j-0.0.tgz new/j-1.0.tgz \
new/i-1.0.tgz old/k-0.0.tgz old/l-0.0.tgz new/l-1.0.tgz old/m-0.0.tgz \
old/n-0.0.tgz new/m-1.0.tgz old/o-0.0.tgz:
	${PKG_CREATE} -f ${.CURDIR}/empty $@

new/o-1.0.tgz:
	${PKG_CREATE} -P'test/p:p-*:p-0.0' -W'coincoin.0.0' -f ${.CURDIR}/empty $@

new/p-0.0.tgz: plist8
	mkdir -p ${SRC13}/lib
	@touch ${SRC13}/lib/libcoincoin.so.0.0
	${PKG_CREATE} -B src13 -DLIBcoincoin_VERSION=0.0 -f plist8 $@


new/b-1.0.tgz:
	${PKG_CREATE} -P'test/a:a-*:a-1.0' -f ${.CURDIR}/empty $@

new/c-0.0.tgz: plist1
	mkdir -p ${SRC1}
	@touch ${SRC1}/${LONG1}
	@touch ${SRC1}/${LONG2}
	@cd ${SRC1} && ln -sf ${LONG2} ${LONG3}
	@cd ${SRC1} && ln -f ${LONG1} ${LONG4}
	${PKG_CREATE} -B src1 -f plist1 $@

new/k-1.0.tgz: plist6
	${PKG_CREATE} -f plist6 $@

new/n-1.0.tgz: plist7
	${PKG_CREATE} -f plist7 $@

new/d-0.0.tgz: plist2
	mkdir -p ${SRC2}
	touch ${SRC2}/a ${SRC2}/b ${SRC2}/c
	echo "coucou" >${SRC2}/f
	echo "not coucou" >${SRC2}/g
	${PKG_CREATE} -B src2 -f plist2 $@

new/e-0.0.tgz: plist3
	mkdir -p ${SRC3}
	touch ${SRC3}/a ${SRC3}/c ${SRC3}/d
	echo "coucou" >${SRC3}/f
	echo "coucou" >${SRC3}/g
	${PKG_CREATE} -B src3 -f plist3 $@

new/f-0.0.tgz:
	${PKG_CREATE} -P'test/d:d-*:d-0.0' -f ${.CURDIR}/empty $@

old/g-0.0.tgz: plist4
	${PKG_CREATE} -f plist4 $@

new/g-0.0.tgz: plist5
	${PKG_CREATE} -f plist5 $@

old/h-0.0.tgz:
	${PKG_CREATE} -P'test/i:i-*:i-0.0' -f ${.CURDIR}/empty $@

new/h-1.0.tgz:
	${PKG_CREATE} -P'test/j:j->=1:j-1.0' -f ${.CURDIR}/empty $@


# some extra tests do not yet pass correctly
.PHONY: ${REGRESS_TARGETS} \
	collision-check1 collision-check2 collision-check4 collision-check5

clean:
	-rm -rf old new dest* plist* src*

.include <bsd.regress.mk>
