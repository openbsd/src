
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#define NEED_newCONSTSUB
#include "ppport.h"

/* Global Data */
 
#define MY_CXT_KEY "Devel::PPPort::_guts" XS_VERSION
 
typedef struct {
    /* Put Global Data in here */
    int dummy;          
} my_cxt_t;
 
START_MY_CXT     

void test1(void)
{
	newCONSTSUB(gv_stashpv("Devel::PPPort", FALSE), "test_value_1", newSViv(1));
}

extern void test2(void);
extern void test3(void);

MODULE = Devel::PPPort		PACKAGE = Devel::PPPort

BOOT:
{
    MY_CXT_INIT;
    /* If any of the fields in the my_cxt_t struct need
       to be initialised, do it here.
     */
    MY_CXT.dummy = 42 ;
}
       
void
test1()

void
test2()

void
test3()

int
test4()
	CODE:
	{
		SV * sv = newSViv(1);
		newRV_inc(sv);
		RETVAL = (SvREFCNT(sv) == 2);
	}
	OUTPUT:
	RETVAL

int
test5()
	CODE:
	{
		SV * sv = newSViv(2);
		newRV_noinc(sv);
		RETVAL = (SvREFCNT(sv) == 1);
	}
	OUTPUT:
	RETVAL

SV *
test6()
	CODE:
	{
		RETVAL = (newSVsv(&PL_sv_undef));
	}
	OUTPUT:
	RETVAL

SV *
test7()
	CODE:
	{
		RETVAL = (newSVsv(&PL_sv_yes));
	}
	OUTPUT:
	RETVAL

SV *
test8()
	CODE:
	{
		RETVAL = (newSVsv(&PL_sv_no));
	}
	OUTPUT:
	RETVAL

int
test9(string)
	char * string;
	CODE:
	{
		PL_na = strlen(string);
		RETVAL = PL_na;
	}
	OUTPUT:
	RETVAL


SV*
test10(value)
	int value
	CODE:
	{
		RETVAL = (newSVsv(boolSV(value)));
	}
	OUTPUT:
	RETVAL


SV*
test11(string, len)
	char * string
	int    len
	CODE:
	{
		RETVAL = newSVpvn(string, len);
	}
	OUTPUT:
	RETVAL

SV*
test12()
	CODE:
	{
		RETVAL = newSVsv(DEFSV);
	}
	OUTPUT:
	RETVAL

int
test13()
	CODE:
	{
		RETVAL = SvTRUE(ERRSV);
	}
	OUTPUT:
	RETVAL

int
test14()
	CODE:
	{
		dMY_CXT;
		RETVAL = (MY_CXT.dummy == 42);
		++ MY_CXT.dummy ;
	}
	OUTPUT:
	RETVAL

int
test15()
	CODE:
	{
		dMY_CXT;
		RETVAL = (MY_CXT.dummy == 43);
	}
	OUTPUT:
	RETVAL

