#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

=for testing

This parts are ignored.

=cut

STATIC void
outlist(int* a, int* b){
	*a = 'a';
	*b = 'b';
}

STATIC int
len(const char* const s, int const l){
	return l;
}

MODULE = XSMore         PACKAGE = XSMore

=for testing

This parts are also ignored.

=cut

PROTOTYPES: ENABLE

VERSIONCHECK: DISABLE

REQUIRE: 2.20

SCOPE: DISABLE

FALLBACK: TRUE

BOOT:
	sv_setiv(get_sv("XSMore::boot_ok", TRUE), 100);


void
prototype_ssa()
PROTOTYPE: $$@
CODE:
	NOOP;

void
attr_method(self, ...)
ATTRS: method
CODE:
	NOOP;

#define RET_1 1
#define RET_2 2

int
return_1()
CASE: ix == 1
	ALIAS:
		return_1 = RET_1
		return_2 = RET_2
	CODE:
		RETVAL = ix;
	OUTPUT:
		RETVAL
CASE: ix == 2
	CODE:
		RETVAL = ix;
	OUTPUT:
		RETVAL

int
arg_init(x)
	int x = SvIV($arg);
CODE:
	RETVAL = x;
OUTPUT:
	RETVAL

int
myabs(...)
OVERLOAD: abs
CODE:
	RETVAL = 42;
OUTPUT:
	RETVAL

void
hook(IN AV* av)
INIT:
	av_push(av, newSVpv("INIT", 0));
CODE:
	av_push(av, newSVpv("CODE", 0));
POSTCALL:
	av_push(av, newSVpv("POSTCALL", 0));
CLEANUP:
	av_push(av, newSVpv("CLEANUP", 0));


void
outlist(OUTLIST int a, OUTLIST int b)

int
len(char* s, int length(s))

#if 1

INCLUDE: XSInclude.xsh

#else

# for testing #else directive

#endif
