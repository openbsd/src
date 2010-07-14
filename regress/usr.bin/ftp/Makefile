# $OpenBSD: Makefile,v 1.2 2010/07/14 06:19:26 halex Exp $

REGRESS_TARGETS= \
		${DASHO_TARGETS:C/^/t-dasho-/} \
		${REDIRECT_TARGETS:C/^/t-redirect-/}

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

DASHO_TARGETS=1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16


rport1?=	9000
rport2?=	9001
rbase1=		http://localhost:${rport1}
rbase2=		http://localhost:${rport2}
rbase_1=	http://[localhost]:${rport1}
rbase_2=	http://[localhost]:${rport2}

redirect=	@rport1=${rport1} rport2=${rport2} sh ${.CURDIR}/redirect.sh

#	${redirect}	request			Location:		request

t-redirect-1:
	${redirect}	${rbase1}/path/to/file	${rbase2}/another/file	${rbase2}/another/file

t-redirect-2:
	${redirect}	${rbase1}/path/to/file	${rbase2}/		${rbase2}/

t-redirect-3:
	${redirect}	${rbase1}/path/to/file	/another/file		${rbase1}/another/file

t-redirect-4:
	${redirect}	${rbase1}/path/to/file	/			${rbase1}/

t-redirect-5:
	${redirect}	${rbase1}/path/to/file	another/file		${rbase1}/path/to/another/file

t-redirect-6:
	${redirect}	${rbase1}/path/to/file/	another/file		${rbase1}/path/to/file/another/file

t-redirect-7:
	${redirect}	${rbase1}/path/to/file	../			${rbase1}/path/to/../

t-redirect-8:
	${redirect}	${rbase1}/path/to/file	${rbase_2}/another/file	${rbase_2}/another/file

t-redirect-9:
	${redirect}	${rbase1}/path/to/file	${rbase_2}/		${rbase_2}/

t-redirect-10:
	${redirect}	${rbase1}/p/x?a#b?c#d	another/file		${rbase1}/p/another/file

t-redirect-11:
	${redirect}	${rbase1}/p/x#b?c#d	another/file		${rbase1}/p/another/file

t-redirect-12:
	${redirect}	${rbase1}/p/x/#b?c#d	another/file		${rbase1}/p/x/another/file

t-redirect-13:
	${redirect}	${rbase1}/p/x?aa#bb	another/file?x#yz	${rbase1}/p/another/file?x

t-redirect-14:
	${redirect}	${rbase1}/p/x/?aa#bb	another/file?x#yz	${rbase1}/p/x/another/file?x

t-redirect-15:
	${redirect}	${rbase1}/p/x#y?z/k#l?m	file?a#b?c#d		${rbase1}/p/file?a

REDIRECT_TARGETS=1 2 3 4 5 6 7 8 9 10 11 12 13 14 15


.include <bsd.regress.mk>
