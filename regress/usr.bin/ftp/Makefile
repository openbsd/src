# $OpenBSD: Makefile,v 1.1 2010/06/29 21:22:03 halex Exp $

DASHO_TARGETS=1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16

REGRESS_TARGETS= \
	${DASHO_TARGETS:C/^/t-dasho-/}

dfile=file:../src/-
xfile=file:../src/X

dasho=	@sh ${.CURDIR}/dasho.sh

#	${dasho} <args>			<$?>	<out>	<err>	<files>

t-dasho-1:
	${dasho} "-V         ${dfile}"	0	""	""	"[-] DASH"

t-dasho-2:
	${dasho} "-V -oX     ${dfile}"	0	""	""	"[X] DASH"

t-dasho-3:
	${dasho} "-V -o- -oX ${dfile}"	0	""	""	"[X] DASH"

t-dasho-4:
	${dasho} "-V -o-     ${dfile}"	0	"DASH"	""	""

t-dasho-5:
	${dasho} "-V -oX -o- ${dfile}"	0	"DASH"	""	""

t-dasho-6:
	${dasho} "-V -o./-   ${dfile}"	0	""	""	"[-] DASH"

t-dasho-7:
	${dasho} "-V         ${xfile}"	0	""	""	"[X] XXXX"

t-dasho-8:
	${dasho} "-V -oX     ${xfile}"	0	""	""	"[X] XXXX"

t-dasho-9:
	${dasho} "-V -o- -oX ${xfile}"	0	""	""	"[X] XXXX"

t-dasho-10:
	${dasho} "-V -o-     ${xfile}"	0	"XXXX"	""	""

t-dasho-11:
	${dasho} "-V -oX -o- ${xfile}"	0	"XXXX"	""	""

t-dasho-12:
	${dasho} "-V -o./-   ${xfile}"	0	""	""	"[-] XXXX"

t-dasho-13:
	${dasho} "-V -oX -o '' ${xfile}" 0	""	""	"[X] XXXX"

t-dasho-14:
	${dasho} "-V -o- -o '' ${dfile}" 0	""	""	"[-] DASH"

t-dasho-15:
	${dasho} "-V -oX -o '' ${xfile}" 0	""	""	"[X] XXXX"

t-dasho-16:
	${dasho} "-V -o- -o '' ${dfile}" 0	""	""	"[-] DASH"

.include <bsd.regress.mk>
