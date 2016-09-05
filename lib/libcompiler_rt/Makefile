# $OpenBSD: Makefile,v 1.1 2016/09/05 11:38:16 pascal Exp $

.include <bsd.own.mk>

LIB=	compiler_rt
NOPIC=
NOPROFILE=

CFLAGS+=	-fPIC -std=gnu99 -fvisibility=hidden -fno-stack-protector
CPPFLAGS+=	-DVISIBILITY_HIDDEN

.if ${MACHINE_ARCH} == "amd64"
RTARCH=	x86_64
.elif ${MACHINE_ARCH} == "powerpc"
RTARCH=	ppc
.else
RTARCH=	${MACHINE_ARCH}
.endif

.PATH: ${.CURDIR}/${RTARCH}

GEN_SRCS=	absvdi2 \
		absvsi2 \
		absvti2 \
		adddf3 \
		addsf3 \
		addtf3 \
		addvdi3 \
		addvsi3 \
		addvti3 \
		apple_versioning \
		ashldi3 \
		ashlti3 \
		ashrdi3 \
		ashrti3 \
		clear_cache \
		clzdi2 \
		clzsi2 \
		clzti2 \
		cmpdi2 \
		cmpti2 \
		comparedf2 \
		comparesf2 \
		cpu_model \
		ctzdi2 \
		ctzsi2 \
		ctzti2 \
		divdc3 \
		divdf3 \
		divdi3 \
		divmoddi4 \
		divmodsi4 \
		divsc3 \
		divsf3 \
		divsi3 \
		divtc3 \
		divti3 \
		divtf3 \
		divxc3 \
		enable_execute_stack \
		eprintf \
		extendsfdf2 \
		extendhfsf2 \
		ffsdi2 \
		ffsti2 \
		fixdfdi \
		fixdfsi \
		fixdfti \
		fixsfdi \
		fixsfsi \
		fixsfti \
		fixunsdfdi \
		fixunsdfsi \
		fixunsdfti \
		fixunssfdi \
		fixunssfsi \
		fixunssfti \
		fixunsxfdi \
		fixunsxfsi \
		fixunsxfti \
		fixxfdi \
		fixxfti \
		floatdidf \
		floatdisf \
		floatdixf \
		floatsidf \
		floatsisf \
		floattidf \
		floattisf \
		floattixf \
		floatundidf \
		floatundisf \
		floatundixf \
		floatunsidf \
		floatunsisf \
		floatuntidf \
		floatuntisf \
		floatuntixf \
		gcc_personality_v0 \
		int_util \
		lshrdi3 \
		lshrti3 \
		moddi3 \
		modsi3 \
		modti3 \
		muldc3 \
		muldf3 \
		muldi3 \
		mulodi4 \
		mulosi4 \
		muloti4 \
		mulsc3 \
		mulsf3 \
		multi3 \
		multf3 \
		mulvdi3 \
		mulvsi3 \
		mulvti3 \
		mulxc3 \
		negdf2 \
		negdi2 \
		negsf2 \
		negti2 \
		negvdi2 \
		negvsi2 \
		negvti2 \
		paritydi2 \
		paritysi2 \
		parityti2 \
		popcountdi2 \
		popcountsi2 \
		popcountti2 \
		powidf2 \
		powisf2 \
		powitf2 \
		powixf2 \
		subdf3 \
		subsf3 \
		subvdi3 \
		subvsi3 \
		subvti3 \
		subtf3 \
		trampoline_setup \
		truncdfhf2 \
		truncdfsf2 \
		truncsfhf2 \
		ucmpdi2 \
		ucmpti2 \
		udivdi3 \
		udivmoddi4 \
		udivmodsi4 \
		udivmodti4 \
		udivsi3 \
		udivti3 \
		umoddi3 \
		umodsi3 \
		umodti3

.for file in ${GEN_SRCS}
.	if exists(${.CURDIR}/${RTARCH}/${file}.S)
SRCS+=	${file}.S
.	else
SRCS+=	${file}.c
.	endif
.endfor

.if ${RTARCH} == "ppc"
SRCS+=	divtc3.c \
	fixtfdi.c \
	fixunstfdi.c \
	floatditf.c \
	floatunditf.c \
	gcc_qadd.c \
	gcc_qdiv.c \
	gcc_qmul.c \
	gcc_qsub.c \
	multc3.c \
	restFP.S \
	saveFP.S
.endif

.include <bsd.lib.mk>
