# $OpenBSD: Makefile,v 1.14 2018/12/25 09:20:49 claudio Exp $

.include <bsd.own.mk>

.if ${COMPILER_VERSION:L} != "clang"
CC=		clang
CXX=		clang++
.endif

.if ${BUILD_CLANG:L} == "yes"

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
		addtf3 \
		addvdi3 \
		addvsi3 \
		addvti3 \
		apple_versioning \
		ashldi3 \
		ashlti3 \
		ashrdi3 \
		ashrti3 \
		atomic \
		bswapdi2 \
		bswapsi2 \
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
		divdi3 \
		divmoddi4 \
		divmodsi4 \
		divsc3 \
		divtc3 \
		divti3 \
		divtf3 \
		divxc3 \
		emutls \
		enable_execute_stack \
		eprintf \
		extendhfsf2 \
		ffsdi2 \
		ffssi2 \
		ffsti2 \
		fixdfti \
		fixsfti \
		fixunsdfsi \
		fixunsdfti \
		fixunssfsi \
		fixunssfti \
		fixunsxfdi \
		fixunsxfsi \
		fixunsxfti \
		fixxfdi \
		fixxfti \
		floattidf \
		floattisf \
		floattixf \
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
		muldi3 \
		mulodi4 \
		mulosi4 \
		muloti4 \
		mulsc3 \
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
		subvdi3 \
		subvsi3 \
		subvti3 \
		subtf3 \
		trampoline_setup \
		truncdfhf2 \
		truncsfhf2 \
		ucmpdi2 \
		ucmpti2 \
		udivdi3 \
		udivmoddi4 \
		udivmodsi4 \
		udivmodti4 \
		udivti3 \
		umoddi3 \
		umodsi3 \
		umodti3

.if ${RTARCH} != "arm"
GEN_SRCS+=	adddf3 \
		addsf3 \
		divdf3 \
		divsf3 \
		divsi3 \
		extendsfdf2 \
		fixdfdi \
		fixdfsi \
		fixsfdi \
		fixsfsi \
		fixunsdfdi \
		fixunssfdi \
		floatsidf \
		floatsisf \
		muldf3 \
		mulsf3 \
		subdf3 \
		subsf3 \
		truncdfsf2 \
		udivsi3
.endif

.if ${RTARCH} == "i386"
SRCS+=	floatdidf.c \
	floatdisf.c \
	floatdixf.c \
	floatundidf.c \
	floatundisf.c \
	floatundixf.c
.else
GEN_SRCS+=	floatdidf \
		floatdisf \
		floatdixf \
		floatundidf \
		floatundisf \
		floatundixf
.endif

.for file in ${GEN_SRCS}
.	if exists(${.CURDIR}/${RTARCH}/${file}.S)
SRCS+=	${file}.S
.	else
SRCS+=	${file}.c
.	endif
.endfor

.if ${RTARCH} == "aarch64"
SRCS+=	comparetf2.c \
	extenddftf2.c \
	extendsftf2.c \
	fixtfdi.c \
	fixtfsi.c \
	fixtfti.c \
	fixunstfdi.c \
	fixunstfsi.c \
	fixunstfti.c \
	floatditf.c \
	floatsitf.c \
	floattitf.c \
	floatunditf.c \
	floatunsitf.c \
	floatuntitf.c \
	multc3.c \
	trunctfdf2.c \
	trunctfsf2.c
.endif

.if ${RTARCH} == "arm"
SRCS+=	aeabi_cdcmp.S \
	aeabi_cdcmpeq_check_nan.c \
	aeabi_cfcmp.S \
	aeabi_cfcmpeq_check_nan.c \
	aeabi_dcmp.S \
	aeabi_div0.c \
	aeabi_drsub.c \
	aeabi_fcmp.S \
	aeabi_frsub.c \
	aeabi_ldivmod.S \
	aeabi_memcmp.S \
	aeabi_memcpy.S \
	aeabi_memmove.S \
	aeabi_memset.S \
	aeabi_uldivmod.S \
	switch16.S \
	switch32.S \
	switch8.S \
	switchu8.S \
	sync_fetch_and_add_4.S \
	sync_fetch_and_add_8.S \
	sync_fetch_and_and_4.S \
	sync_fetch_and_and_8.S \
	sync_fetch_and_max_4.S \
	sync_fetch_and_max_8.S \
	sync_fetch_and_min_4.S \
	sync_fetch_and_min_8.S \
	sync_fetch_and_nand_4.S \
	sync_fetch_and_nand_8.S \
	sync_fetch_and_or_4.S \
	sync_fetch_and_or_8.S \
	sync_fetch_and_sub_4.S \
	sync_fetch_and_sub_8.S \
	sync_fetch_and_umax_4.S \
	sync_fetch_and_umax_8.S \
	sync_fetch_and_umin_4.S \
	sync_fetch_and_umin_8.S \
	sync_fetch_and_xor_4.S \
	sync_fetch_and_xor_8.S \
	sync_synchronize.S
.endif

.if ${RTARCH} == "mips64" || ${RTARCH} == "mips64el"
SRCS+=	comparetf2.c \
	extenddftf2.c \
	extendsftf2.c \
	fixtfdi.c \
	fixtfsi.c \
	fixtfti.c \
	fixunstfdi.c \
	fixunstfsi.c \
	fixunstfti.c \
	floatditf.c \
	floatsitf.c \
	floattitf.c \
	floatunditf.c \
	floatunsitf.c \
	floatuntitf.c \
	multc3.c \
	trunctfdf2.c \
	trunctfsf2.c
.endif

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
	multc3.c
.endif

.if ${RTARCH} == "sparc64"
SRCS+=	comparetf2.c \
	extenddftf2.c \
	extendsftf2.c \
	fixtfdi.c \
	fixtfsi.c \
	fixtfti.c \
	fixunstfdi.c \
	fixunstfsi.c \
	fixunstfti.c \
	floatditf.c \
	floatsitf.c \
	floattitf.c \
	floatunditf.c \
	floatunsitf.c \
	floatuntitf.c \
	multc3.c \
	trunctfdf2.c \
	trunctfsf2.c
.endif

.include <bsd.lib.mk>

.else
NOPROG=
.include <bsd.prog.mk>
.endif
