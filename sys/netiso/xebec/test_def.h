/*	$NetBSD: test_def.h,v 1.4 1994/06/29 06:41:20 cgd Exp $	*/

struct blah {
	unsigned int blahfield;
	int		dummyi;
	char 	dummyc;
};

struct test_pcbstruct {
	int test_pcbfield;
	int test_state;
};

#define MACRO1(arg) if(arg != 0) { printf("macro1\n"); }
