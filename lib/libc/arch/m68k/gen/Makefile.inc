#	$OpenBSD: Makefile.inc,v 1.10 2011/07/08 22:28:33 martynas Exp $

SRCS+=	_setjmp.S fabs.S infinity.c ldexp.S \
	modf.S nan.c setjmp.S sigsetjmp.S
SRCS+=	flt_rounds.S fpgetmask.S fpgetround.S fpgetsticky.S fpsetmask.S \
	fpsetround.S fpsetsticky.S
SRCS+=	fpclassifyl.c isfinitel.c isinfl.c isnanl.c isnormall.c signbitl.c
SRCS+=	adddf3.S addsf3.S ashlsi3.S ashrsi3.S cmpdf2.S cmpsf2.S divdf3.S \
	divsf3.S divsi3.S extendsfdf2.S fixdfsi.S fixunsdfsi.S \
	floatsidf.S lshlsi3.S lshrsi3.S modsi3.S muldf3.S mulsf3.S mulsi3.S \
	negdf2.S negsf2.S saveregs.c subdf3.S subsf3.S truncdfsf2.S udivsi3.S \
	umodsi3.S umulsi3.S

SRCS+=	alloca.S
LSRCS+= alloca.c
