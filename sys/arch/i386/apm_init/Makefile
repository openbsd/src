#
# LP (Laptop Package)
#
# (C) 1994 by HOSOKAWA, Tatsumi <hosokawa@mt.cs.keio.ac.jp>
#
# This software may be used, modified, copied, and distributed in
# both source and binary form provided that the above copyright and
# these terms are retained. Under no circumstances is the author 
# responsible for the proper functioning of this software, nor does 
# the author assume any responsibility for damages incurred with its 
# use.
#
# Sep., 1994	Implemented on FreeBSD 1.1.5.1R (Toshiba AVS001WD)
# Oct., 1994	NetBSD port (1.0 BETA 10/2) by ukai
# Dec., 1995	NetBSD 1.1 kernel build retrofit, jtk@netbsd.org
#

.if exists (${APMDIR}/arch/${MACHINE_ARCH}/Makefile.inc)
.PATH:	${APMDIR}/arch/${MACHINE_ARCH}
.include "${APMDIR}/arch/${MACHINE_ARCH}/Makefile.inc"
.endif

.PATH: ${APMDIR}

CC = ${APMCC}
CFLAGS += -DINITIALIZER	-I${APMDIR} -DKERNEL \
	${APMCFLAGS:S@-I.@-I${KERNREL}.@g}

OBJS = apm_init.o real_prot.o table.o

#.SUFFIXES: .c .S .o
#
#.c.o:
#	$(CC)  $(CFLAGS) $(OPTFLAGS) $(INC) -c $<
#
.S.o:
	$(CC)  $(CFLAGS) $(INC) -c $<

apm_init.inc: apm_init
	sh ${APMDIR}/bin2asm.sh apm_init > apm_init.inc

apm_init: $(OBJS)
	$(LD) -Bstatic -N -T 0 -o apm_init $(OBJS)
	cp apm_init apm_init.sym
	@strip apm_init
	@sh ${APMDIR}/rmaouthdr apm_init apm_init.tmp
	@mv -f apm_init.tmp apm_init

#allclean: clean
#	@rm -f apm_init.inc
clean:
	rm -f *.o apm_init apm_init.sym apm_init.inc

#.include <bsd.prog.mk>
