#	$OpenBSD: Makefile.arc,v 1.13 1999/08/15 20:43:57 niklas Exp $

#	@(#)Makefile.arc	8.2 (Berkeley) 2/16/94
#
# Makefile for 4.4 BSD
#
# This makefile is constructed from a machine description:
#	config ``machineid''
# Most changes should be made in the machine description
#	/sys/arch/arc/conf/``machineid''
# after which you should do
#	 config ``machineid''
# Machine generic makefile changes should be made in
#	/sys/arch/arc/conf/Makefile.``machinetype''
# after which config should be rerun for all machines of that type.
#
# N.B.: NO DEPENDENCIES ON FOLLOWING FLAGS ARE VISIBLE TO MAKEFILE
#	IF YOU CHANGE THE DEFINITION OF ANY OF THESE RECOMPILE EVERYTHING
#
# -DTRACE	compile in kernel tracing hooks
# -DQUOTA	compile in file system quotas


# DEBUG is set to -g by config if debugging is requested (config -g).
# PROF is set to -pg by config if profiling is requested (config -p).

AS?=	as
CC?=	cc
CPP?=	cpp
LD?=	ld
STRIP?=	strip -d
TOUCH?=	touch -f -c

# source tree is located via $S relative to the compilation directory
S=	../../../..
ARC=	../..
MIPS=	../../../mips

INCLUDES=	-I. -I$S/arch -I$S
CPPFLAGS=	${INCLUDES} ${IDENT} -D_KERNEL -Darc
CDIAGFLAGS=	-Werror -Wall -Wmissing-prototypes -Wstrict-prototypes \
		-Wno-uninitialized -Wno-format -Wno-main

CFLAGS=		${DEBUG} -O2 ${CDIAGFLAGS} -mno-abicalls -mips2 -mcpu=r4000 \
		${COPTS}
AFLAGS=		-x assembler-with-cpp -traditional-cpp -D_LOCORE

### find out what to use for libkern
.include "$S/lib/libkern/Makefile.inc"
.ifndef PROF
LIBKERN=	${KERNLIB}
.else
LIBKERN=	${KERNLIB_PROF}
.endif

### find out what to use for libcompat
.include "$S/compat/common/Makefile.inc"
.ifndef PROF
LIBCOMPAT=	${COMPATLIB}
.else
LIBCOMPAT=	${COMPATLIB_PROF}
.endif

# compile rules: rules are named ${TYPE}_${SUFFIX}${CONFIG_DEP}
# where TYPE is NORMAL, DRIVER, or PROFILE}; SUFFIX is the file suffix,
# capitalized (e.g. C for a .c file), and CONFIG_DEP is _C if the file
# is marked as config-dependent.

USRLAND_C=	${CC} ${CFLAGS} ${CPPFLAGS} ${PROF} -c $<
USRLAND_C_C=	${CC} ${CFLAGS} ${CPPFLAGS} ${PROF} ${PARAM} -c $<

NORMAL_C=	${CC} ${CFLAGS} ${CPPFLAGS} ${PROF} -c $<
NORMAL_C_C=	${CC} ${CFLAGS} ${CPPFLAGS} ${PROF} ${PARAM} -c $<

DRIVER_C=	${CC} ${CFLAGS} ${CPPFLAGS} ${PROF} -c $<
DRIVER_C_C=	${CC} ${CFLAGS} ${CPPFLAGS} ${PROF} ${PARAM} -c $<

NORMAL_S=	${CC} ${AFLAGS} ${CPPFLAGS} -c $<
NORMAL_S_C=	${AS}  ${COPTS} ${PARAM} $< -o $@

%OBJS

%CFILES

%SFILES

# load lines for config "xxx" will be emitted as:
# xxx: ${SYSTEM_DEP} swapxxx.o
#	${SYSTEM_LD_HEAD}
#	${SYSTEM_LD} swapxxx.o
#	${SYSTEM_LD_TAIL}

SYSTEM_OBJ=	locore.o fp.o ${OBJS} param.o ioconf.o ${LIBKERN} \
		${LIBCOMPAT}
#
SYSTEM_DEP=	Makefile ${SYSTEM_OBJ}
SYSTEM_LD_HEAD=	rm -f $@
SYSTEM_LD=	-@if [ X${DEBUG} = X-g ]; \
		then strip=-X; \
		else strip=-x; \
		fi; \
		echo ${LD} $$strip -o $@ -e start -T ../../conf/ld.script \
			'$${SYSTEM_OBJ}' vers.o; \
		${LD} $$strip -o $@ -e start -T ../../conf/ld.script \
			${SYSTEM_OBJ} vers.o
#
SYSTEM_LD_TAIL=	chmod 755 $@; \
		elf2ecoff $@ $@.ecoff; \
		size $@

%LOAD

newvers:
	sh $S/conf/newvers.sh
	${CC} $(CFLAGS) -c vers.c

clean::
	rm -f eddep bsd bsd.gdb bsd.ecoff tags *.o locore.i [a-z]*.s \
	    Errs errs linterrs makelinks 

lint: /tmp param.c
	@lint -hbxn -DGENERIC -Dvolatile= ${COPTS} ${PARAM} -UKGDB \
	    ${CFILES} ioconf.c param.c

symbols.sort: ${ARC}/arc/symbols.raw
	grep -v '^#' ${ARC}/arc/symbols.raw \
	    | sed 's/^	//' | sort -u > symbols.sort

locore.o: ${ARC}/arc/locore.S ${MIPS}/include/asm.h \
	${MIPS}/include/cpu.h ${MIPS}/include/reg.h assym.h
	${NORMAL_S} -mips3 ${ARC}/arc/locore.S

fp.o: ${MIPS}/mips/fp.S ${MIPS}/include/asm.h \
	${ARC}/include/cpu.h ${MIPS}/include/reg.h assym.h
	${NORMAL_S} -mips3 ${MIPS}/mips/fp.S

# the following are necessary because the files depend on the types of
# cpu's included in the system configuration
clock.o machdep.o autoconf.o conf.o: Makefile

# depend on network configuration
uipc_domain.o uipc_proto.o vfs_conf.o: Makefile
if_tun.o if_loop.o if_ethersubr.o: Makefile
in_proto.o: Makefile


assym.h: $S/kern/genassym.sh ${ARC}/arc/genassym.cf
	sh $S/kern/genassym.sh ${CC} ${CFLAGS} ${CPPFLAGS} \
	    ${PARAM} < ${ARC}/arc/genassym.cf > assym.h.tmp && \
	    mv -f assym.h.tmp assym.h


links:
	egrep '#if' ${CFILES} | sed -f $S/conf/defines | \
	  sed -e 's/:.*//' -e 's/\.c/.o/' | sort -u > dontlink
	echo ${CFILES} | tr -s ' ' '\12' | sed 's/\.c/.o/' | \
	  sort -u | comm -23 - dontlink | \
	  sed 's,../.*/\(.*.o\),rm -f \1;ln -s ../GENERIC/\1 \1,' > makelinks
	sh makelinks && rm -f dontlink

tags:
	@echo "see $S/kern/Makefile for tags"

ioconf.o: ioconf.c
	${NORMAL_C}

param.c: $S/conf/param.c
	rm -f param.c
	cp $S/conf/param.c .

param.o: param.c Makefile
	${NORMAL_C_C}

newvers: ${SYSTEM_DEP} ${SYSTEM_SWAP_DEP}
	sh $S/conf/newvers.sh
	${CC} ${CFLAGS} ${CPPFLAGS} ${PROF} -c vers.c

depend:: .depend
.depend: ${SRCS} assym.h param.c
	mkdep ${AFLAGS} ${CPPFLAGS} ${ARC}/arc/locore.s
	mkdep -a ${CFLAGS} ${CPPFLAGS} param.c ioconf.c ${CFILES}
	mkdep -a ${AFLAGS} ${CPPFLAGS} ${SFILES}

%RULES

