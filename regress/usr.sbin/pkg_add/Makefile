# $OpenBSD: Makefile,v 1.35 2010/01/14 19:38:37 espie Exp $

REGRESS_TARGETS=pkgnames signatures depends-check longnames update-check1 \
	collision-check3 collision-check5 partial-update-test conflict-update \
	merge-update split-update big-merge family-circus missing \
	lib-report1 lib-report2 lib-report3 loop1

PKG_ADD=perl ${.CURDIR}/myadd
PKG_CREATE=perl ${.CURDIR}/mycreate
LONG1=iamareallongfilenamethatcantberepresentedcorrectlyinatararchivebutwewantittohappenanywaysohmygodaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa42
LONG2=iamanotherreallygfilenamethatcantberepresentedcorrectlyinatararchivebutwewantittohappenanywaysohmygodaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa42
LONG3=iamanotherreallylonglinkthatcantberepresentedcorrectlyinatararchivebutwewantittohappenanywaysohmygodaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa42
LONG4=iamanotherreallylonghardlinkthatcantberepresentedcorrectlyinatararchivebutwewantittohappenanywaysohmygodaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa42

.for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25
S$i ?= ${.OBJDIR}/src$i
SRC$i ?= ${S$i}/usr/local
D$i ?= ${.OBJDIR}/dest$i
DEST$i ?= ${D$i}/usr/local
.endfor

pkgnames:
	perl ${.CURDIR}/check-name

signatures: 
	-rm -f signatures.out
	perl ${.CURDIR}/check-sig 2>signatures.out
	diff -u signatures.out ${.CURDIR}/signatures.ref

depends-check: rep0/a-0.tgz rep0/b-0.tgz rep1/a-1.tgz rep1/b-1.tgz
	@-rm -rf ${D1}
	@ROOT=${D1} ${PKG_ADD} rep0/a-0.tgz rep0/b-0.tgz
	@ROOT=${D1} PKG_PATH=${.OBJDIR}/rep1 ${PKG_ADD} -u a b
	@ROOT=${D1} PKG_PATH=${.OBJDIR}/rep0 ${PKG_ADD} -D downgrade -u a b

longnames: rep1/c-0.tgz
	@-rm -rf ${D2}
	@ROOT=${D2} ${PKG_ADD} rep1/c-0.tgz
	@test -f ${DEST2}/${LONG1}
	@test -f ${DEST2}/${LONG2}
	@cd ${DEST2} && test -h ${LONG3} && test `readlink ${LONG3}` = ${LONG2}
	@cd ${DEST2} && test `stat -f '%i' ${LONG1}` = `stat -f '%i' ${LONG4}`

collision-check1: rep1/d-0.tgz rep1/e-0.tgz
	@-rm -rf ${D3}
	@ROOT=${D3} ${PKG_ADD} rep1/d-0.tgz
	@ROOT=${D3} ${PKG_ADD} rep1/e-0.tgz

collision-check2: rep1/d-0.tgz rep1/e-0.tgz
	@-rm -rf ${D4}
	@ROOT=${D4} ${PKG_ADD} rep1/d-0.tgz
	@-rm -rf ${D4}/pkgdb/d-0
	@ROOT=${D4} ${PKG_ADD} rep1/e-0.tgz

collision-check3: rep1/d-0.tgz rep1/f-0.tgz
	@-rm -rf ${D5}
	@ROOT=${D5} ${PKG_ADD} rep1/d-0.tgz rep1/f-0.tgz
	@-rm -rf ${D5}/pkgdb/d-0
	@ROOT=${D5} ${PKG_ADD} -D repair rep1/d-0.tgz
	PKG_DBDIR=${D5}/pkgdb pkg_info -qR d

collision-check4: rep1/d-0.tgz rep1/e-0.tgz
	@-rm -rf ${D6}
	@ROOT=${D6} ${PKG_ADD} rep1/d-0.tgz
	@-rm -rf ${D6}/pkgdb/d-0
	@ROOT=${D6} ${PKG_ADD} -D repair rep1/e-0.tgz

collision-check5: rep1/d-0.tgz rep1/e-0.tgz
	@-rm -rf ${D21}
	@ROOT=${D21} ${PKG_ADD} -n rep1/d-0.tgz rep1/e-0.tgz

collision-check5: rep1/d-0.tgz rep1/e-0.tgz
	@-rm -rf ${D7}
	@ROOT=${D7} ${PKG_ADD} rep1/d-0.tgz
	@-rm -rf ${D7}/pkgdb/d-0
	@ROOT=${D7} ${PKG_ADD} -D removecollisions rep1/d-0.tgz

update-check1: rep0/g-0.tgz rep1/g-0.tgz rep0/ga-0.tgz rep1/ga-0.tgz \
	rep0/gb-0.tgz rep1/gb-0p0.tgz rep0/gc-0.tgz rep1/gc-0.tgz \
	rep0/gd-0.tgz rep1/gd-1.tgz rep0/ge-0.tgz rep1/ge-1.tgz \
	rep0/gf-1.tgz rep1/gf-0.tgz
	@-rm -rf ${D8}
	@ROOT=${D8} PKG_PATH=${.OBJDIR}/rep0 ${PKG_ADD} g ga gb gc gd ge gf
	@ROOT=${D8} PKG_PATH=${.OBJDIR}/rep1 ${PKG_ADD} -u
	@PKG_DBDIR=${D8}/pkgdb pkg_info |diff - ${.CURDIR}/list7.out
	@ROOT=${D8} PKG_PATH=${.OBJDIR}/rep1 ${PKG_ADD} -u gd ge
	@PKG_DBDIR=${D8}/pkgdb pkg_info |diff - ${.CURDIR}/list8.out

list-check: rep1/a-1.tgz rep1/b-1.tgz rep1/c-0.tgz rep1/d-0.tgz \
	rep1/e-0.tgz rep1/f-0.tgz rep1/g-0.tgz
	@-rm -rf ${D9}
	@ROOT=${D9} PKG_PATH=${.OBJDIR}/rep1 ${PKG_ADD} -l ${.CURDIR}/list
	@PKG_DBDIR=${D9}/pkgdb pkg_info -q|diff - ${.CURDIR}/list.out

partial-update-test: rep0/h-0.tgz rep0/i-0.tgz rep0/j-0.tgz \
	rep1/h-1.tgz rep1/i-1.tgz rep1/j-1.tgz
	@-rm -rf ${D10}
	@ROOT=${D10} PKG_PATH=${.OBJDIR}/rep0 ${PKG_ADD} h j
	@ROOT=${D10} PKG_PATH=${.OBJDIR}/rep1 ${PKG_ADD} -u h
	@PKG_DBDIR=${D10}/pkgdb pkg_info -q|diff - ${.CURDIR}/list2.out

conflict-update: rep0/k-0.tgz rep0/l-0.tgz rep1/k-1.tgz rep1/l-1.tgz
	@-rm -rf ${D11}
	@ROOT=${D11} PKG_PATH=${.OBJDIR}/rep0 ${PKG_ADD} k l
	@ROOT=${D11} PKG_PATH=${.OBJDIR}/rep1 ${PKG_ADD} -u k
	@PKG_DBDIR=${D11}/pkgdb pkg_info -q|diff - ${.CURDIR}/list3.out

merge-update: rep0/m-0.tgz rep0/n-0.tgz rep1/m-1.tgz rep1/n-1.tgz
	@-rm -rf ${D12}
	@ROOT=${D12} PKG_PATH=${.OBJDIR}/rep0 ${PKG_ADD} m n
	@ROOT=${D12} PKG_PATH=${.OBJDIR}/rep1 ${PKG_ADD} -u n
	@PKG_DBDIR=${D12}/pkgdb pkg_info -q|diff - ${.CURDIR}/list4.out

split-update: rep0/o-0.tgz rep1/o-1.tgz rep1/p-0.tgz
	@-rm -rf ${D13}
	@ROOT=${D13} PKG_PATH=${.OBJDIR}/rep0 ${PKG_ADD} o
	@ROOT=${D13} PKG_PATH=${.OBJDIR}/rep1 ${PKG_ADD} -u o
	@PKG_DBDIR=${D13}/pkgdb pkg_info -q |diff - ${.CURDIR}/list5.out

big-merge: rep0/q1-0.tgz rep0/q2-0.tgz rep0/q3-0.tgz rep0/q4-0.tgz \
	rep0/q5-0.tgz rep0/q6-0.tgz rep1/q5-1.tgz rep1/q6-1.tgz \
	rep1/q1-1.tgz rep1/q2-1.tgz rep1/q3-1.tgz rep1/q4-1.tgz
	@-rm -rf ${D14}
	@ROOT=${D14} PKG_PATH=${.OBJDIR}/rep0 ${PKG_ADD} q1 q2 q3 q4 q5 q6
	@ROOT=${D14} PKG_PATH=${.OBJDIR}/rep1 ${PKG_ADD} -x -D update -u 2>&1 |fgrep XXX|fgrep -v @|diff - ${.CURDIR}/list6.out

family-circus: rep0/glib-0.tgz rep0/fam-0.tgz rep1/fam-1.tgz rep1/glib-1.tgz rep1/gamin-0.tgz
	@-rm -rf ${D15}
	@ROOT=${D15} PKG_PATH=${.OBJDIR}/rep0 ${PKG_ADD} glib
	@ROOT=${D15} PKG_PATH=${.OBJDIR}/rep1 ${PKG_ADD} -u glib

missing: rep1/missa-0.tgz rep1/missb-0.tgz rep1/missc-0.tgz rep1/missd-0.tgz rep1/missf-0.tgz
	@-rm -rf ${D16}
	@ROOT=${D16} PKG_PATH=${.OBJDIR}/rep1 ${PKG_ADD} missc
	@-ROOT=${D15} PKG_PATH=${.OBJDIR}/rep1 ${PKG_ADD} missa missb missf

lib-report1: rep0/o1-0.tgz rep1/o1-1.tgz rep1/p1-0.tgz
	@-rm -rf ${D17}
	@ROOT=${D17} PKG_PATH=${.OBJDIR}/rep0 ${PKG_ADD} o1
	-@ROOT=${D17} PKG_PATH=${.OBJDIR}/rep1 ${PKG_ADD} -u o1

lib-report2: rep0/o2-0.tgz rep1/o2-1.tgz rep1/p2-0.tgz
	@-rm -rf ${D18}
	@ROOT=${D18} PKG_PATH=${.OBJDIR}/rep0 ${PKG_ADD} o2
	-@ROOT=${D18} PKG_PATH=${.OBJDIR}/rep1 ${PKG_ADD} -u o2

lib-report3: rep0/o3-0.tgz
	@-rm -rf ${D19}
	-@ROOT=${D19} PKG_PATH=${.OBJDIR}/rep0 ${PKG_ADD} o3

loop1: rep0/ocaml-3.11.1.tgz rep0/tcl-8.5.7.tgz rep0/tk-8.5.7.tgz
	@-rm -rf ${D20}
	-@ROOT=${D20} PKG_PATH=${.OBJDIR}/rep0 ${PKG_ADD} -z ocaml-3.11.1p0 tcl-8.5.8 tk-8.5.8

plist1:
	@echo "@owner "`id -un` >$@
	@echo "@group "`id -gn` >>$@
	@echo ${LONG1} >>$@
	@echo ${LONG2} >>$@
	@echo ${LONG3} >>$@
	@echo ${LONG4} >>$@

plist2:
	@echo "@owner "`id -un` >$@
	@echo "@group "`id -gn` >>$@
	@echo a >>$@
	@echo b >>$@
	@echo c >>$@
	@echo f >>$@
	@echo g >>$@

plist3:
	@echo "@owner "`id -un` >$@
	@echo "@group "`id -gn` >>$@
	@echo a >>$@
	@echo c >>$@
	@echo d >>$@
	@echo f >>$@
	@echo g >>$@

plist4:
	@echo "@option always-update" >$@

plist5:
	@echo "@option explicit-update" >$@

plist6:
	@echo "@conflict l-<1" >$@

plist7:
	@echo "@conflict m-<1" >$@
	@echo "@pkgpath t/m" >>$@

plist8:
	@echo "@conflict o-<1" >$@
	@echo "@pkgpath t/o" >>$@
	@echo "@owner "`id -un` >>$@
	@echo "@group "`id -gn` >>$@
	@echo '@lib lib/libcoincoin.so.$${LIBcoincoin_VERSION}' >>$@

plist9:
	@echo "@conflict gamin-*" >$@

plist10:
	@echo "@conflict fam-*" >$@
	@echo "@pkgpath t/fam" >>$@

plist11:
	@echo "@conflict missc-*" >$@

rep0/a-0.tgz:
	@${PKG_CREATE} -P't/b:b-*:b-0' -f ${.CURDIR}/empty $@

rep1/a-1.tgz rep0/b-0.tgz rep0/i-0.tgz rep0/j-0.tgz rep1/j-1.tgz \
rep1/i-1.tgz rep0/k-0.tgz rep0/l-0.tgz rep1/l-1.tgz rep0/m-0.tgz \
rep0/n-0.tgz rep1/m-1.tgz rep0/o-0.tgz rep0/fam-0.tgz \
rep0/o1-0.tgz rep0/o2-0.tgz rep0/ga-0.tgz \
rep0/gb-0.tgz rep1/gb-0p0.tgz rep1/gd-1.tgz rep0/ge-0.tgz \
rep0/gf-1.tgz rep1/gf-0.tgz \
rep1/missc-0.tgz rep0/q5-0.tgz rep1/q5-1.tgz \
rep0/tcl-8.5.7.tgz:
	@${PKG_CREATE} -f ${.CURDIR}/empty $@

rep1/o-1.tgz:
	@${PKG_CREATE} -P't/p:p-*:p-0' -W'coincoin.0.0' -f ${.CURDIR}/empty $@

rep1/o1-1.tgz:
	@${PKG_CREATE} -DREGRESSION_TESTING -P't/p1:p1-*:p1-0' -W'coincoin.0' -f ${.CURDIR}/empty $@

rep1/o2-1.tgz:
	@${PKG_CREATE} -DREGRESSION_TESTING -P't/p2:p2-*:p2-0' -W'coincoin.0' -f ${.CURDIR}/empty $@

rep0/o3-0.tgz:
	@${PKG_CREATE} -W'unlikelylibraryname.0.0' -f ${.CURDIR}/empty $@

rep1/p-0.tgz rep1/p1-0.tgz: plist8
	@mkdir -p ${SRC13}/lib
	@touch ${SRC13}/lib/libcoincoin.so.0.0
	@${PKG_CREATE} -B src13 -DLIBcoincoin_VERSION=0.0 -f plist8 $@

rep1/p2-0.tgz: plist8
	@mkdir -p ${SRC14}/lib
	@touch ${SRC14}/lib/libcoincoin.so.0
	@${PKG_CREATE} -DREGRESSION_TESTING -B src14 -DLIBcoincoin_VERSION=0 -f plist8 $@

rep1/b-1.tgz:
	@${PKG_CREATE} -P't/a:a-*:a-1' -f ${.CURDIR}/empty $@

rep1/c-0.tgz: plist1
	@mkdir -p ${SRC1}
	@touch ${SRC1}/${LONG1}
	@touch ${SRC1}/${LONG2}
	@cd ${SRC1} && ln -sf ${LONG2} ${LONG3}
	@cd ${SRC1} && ln -f ${LONG1} ${LONG4}
	@${PKG_CREATE} -B src1 -f plist1 $@

rep1/k-1.tgz: plist6
	@${PKG_CREATE} -f plist6 $@

rep1/n-1.tgz: plist7
	@${PKG_CREATE} -f plist7 $@

rep1/d-0.tgz: plist2
	@mkdir -p ${SRC2}
	@touch ${SRC2}/a ${SRC2}/b ${SRC2}/c
	@echo "coucou" >${SRC2}/f
	@echo "not coucou" >${SRC2}/g
	@${PKG_CREATE} -B src2 -f plist2 $@

rep1/e-0.tgz: plist3
	@mkdir -p ${SRC3}
	@touch ${SRC3}/a ${SRC3}/c ${SRC3}/d
	@echo "coucou" >${SRC3}/f
	@echo "coucou" >${SRC3}/g
	@${PKG_CREATE} -B src3 -f plist3 $@

rep1/f-0.tgz:
	@${PKG_CREATE} -P't/d:d-*:d-0' -f ${.CURDIR}/empty $@

rep0/g-0.tgz: plist4
	@${PKG_CREATE} -f plist4 $@

rep1/g-0.tgz: plist4
	@PACKAGE_COMMENT=updated ${PKG_CREATE} -f plist4 $@

rep0/gd-0.tgz rep1/ge-1.tgz: plist5
	@${PKG_CREATE} -f plist5 $@

rep1/ga-0.tgz:
	@PACKAGE_COMMENT=updated ${PKG_CREATE} -f ${.CURDIR}/empty $@

rep0/gc-0.tgz:
	@${PKG_CREATE} -P't/ga:gb-*:gb-0' -f ${.CURDIR}/empty $@

rep1/gc-0.tgz:
	@PACKAGE_COMMENT=updated ${PKG_CREATE} -P't/ga:gb-*:gb-0p0' -f ${.CURDIR}/empty $@

rep0/h-0.tgz:
	@${PKG_CREATE} -P't/i:i-*:i-0' -f ${.CURDIR}/empty $@

rep1/h-1.tgz:
	@${PKG_CREATE} -P't/j:j->=1:j-1' -f ${.CURDIR}/empty $@

depend_q1=-P't/q5:q5-*:q5-0'
depend_q2=${depend_q1} -P't/q1:q1-*:q1-0'
depend_q3=${depend_q1} -P't/q2:q2-*:q2-0'
depend_q4=${depend_q1} -P't/q3:q3-*:q3-0'

.for n in q1 q2 q3 q4
plist-rep0-$n:
	@echo "@unexec echo 1>&2 'XXXrep0 $n'" >$@

rep0/$n-0.tgz: plist-rep0-$n
	@${PKG_CREATE} ${depend_$n} -f plist-rep0-$n $@
rep1/$n-1.tgz: plist-rep1-$n
	@${PKG_CREATE} ${depend_$n} -f plist-rep1-$n $@

plist-rep1-$n:
	@echo "@conflict q1-0" >$@
	@echo "@conflict q2-0" >>$@
	@echo "@conflict q3-0" >>$@
	@echo "@conflict q4-0" >>$@
	@echo "@exec echo 1>&2 'XXXrep1 $n'" >>$@

.endfor
	
rep0/q6-0.tgz:
	@${PKG_CREATE} -P't/q3:q3-*:q3-0' -f ${.CURDIR}/empty $@

rep1/q6-1.tgz:
	@${PKG_CREATE} -P't/q3:q3-*:q3-0' -f ${.CURDIR}/empty $@

rep0/glib-0.tgz:
	@${PKG_CREATE} -P't/fam:fam-*:fam-0' -f ${.CURDIR}/empty $@

rep1/glib-1.tgz:
	@${PKG_CREATE} -P't/gamin:gamin-*:gamin-0' -f ${.CURDIR}/empty $@

rep1/fam-1.tgz: plist9
	@${PKG_CREATE} -f plist9 $@

rep1/gamin-0.tgz: plist10
	@${PKG_CREATE} -f plist10 $@

rep1/missa-0.tgz:
	@${PKG_CREATE} -P't/misse:misse-*:misse-0' -f ${.CURDIR}/empty $@

rep1/missb-0.tgz:
	@${PKG_CREATE} -P't/missd:missd-*:missd-0' -f ${.CURDIR}/empty $@

rep1/missd-0.tgz: plist11
	@${PKG_CREATE} -f plist11 $@

rep1/missf-0.tgz:
	@${PKG_CREATE} -P't/misse:misse-*:misse-0' -f ${.CURDIR}/empty $@

rep0/ocaml-3.11.1.tgz:
	@${PKG_CREATE} -P'test/tcl:tcl->=8.5,<8.6:tcl-8.5.7' \
		-P'test/tk:tk->=8.5,<8.6:tk-8.5.7' -f ${.CURDIR}/empty $@

rep0/tk-8.5.7.tgz:
	@${PKG_CREATE} -P'test/tcl:tcl->=8.5.7,<8.5.8:tcl-8.5.7' \
		-f ${.CURDIR}/empty $@

# some extra tests do not yet pass correctly
.PHONY: ${REGRESS_TARGETS} \
	collision-check1 collision-check2 collision-check4 collision-check5

clean:
	-rm -rf rep* dest* plist* src* signatures.out

.include <bsd.regress.mk>
