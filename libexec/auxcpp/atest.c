#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <setjmp.h>

#if defined TEST_NATIVE

#define NATIVE_SIGNED             int
#define NATIVE_UNSIGNED           unsigned

#define NATIVE_UNSIGNED_BITS      32
#define NATIVE_SIGNED_MIN         LONG_MIN
#define NATIVE_SIGNED_MAX         LONG_MAX

#elif defined TEST_SIMUL

#define SIMUL_ARITH_SUBTYPE       unsigned short
#define SIMUL_SUBTYPE_BITS        16
#define SIMUL_NUMBITS             31

#else

#error ====== Either TEST_NATIVE or TEST_SIMUL must be defined.

#endif

#define ARITH_TYPENAME            zoinx
#define ARITH_FUNCTION_HEADER     static inline

#define ARITH_WARNING(type)       z_warn(type)
#define ARITH_ERROR(type)         z_error(type)

void z_warn(int type);
void z_error(int type);

#include "arith.c"

#if defined TEST_NATIVE

static inline u_zoinx unsigned_to_uz(unsigned x)
{
	return (u_zoinx)x;
}

static inline s_zoinx int_to_sz(int x)
{
	return (s_zoinx)x;
}

static inline void print_uz(u_zoinx x)
{
	printf("%u", x);
}

static inline void print_sz(s_zoinx x)
{
	printf("%d", x);
}

#else

static inline u_zoinx unsigned_to_uz(unsigned x)
{
	u_zoinx v;
	v.msw = (x >> 16) & 0x7FFFU;
	v.lsw = x & 0xFFFFU;
	return v;
}

static inline s_zoinx int_to_sz(int x)
{
	return unsigned_to_uz((unsigned)x);
}

static inline void print_uz(u_zoinx x)
{
	printf("%u", ((unsigned)(x.msw) << 16) + (unsigned)(x.lsw));
}

static inline void print_sz(s_zoinx x)
{
	if (x.msw & 0x4000U) {
		putchar('-');
		x = zoinx_u_neg(x);
	}
	print_uz(x);
}

#endif

static inline void print_int(int x)
{
	printf("%d", x);
}

static jmp_buf jbuf;

void z_warn(int type)
{
	switch (type) {
	case ARITH_EXCEP_CONV_O:
		fputs("[overflow on conversion] ", stdout); break;
	case ARITH_EXCEP_NEG_O:
		fputs("[overflow on unary minus] ", stdout); break;
	case ARITH_EXCEP_NOT_T:
		fputs("[trap representation on bitwise inversion] ", stdout);
		break;
	case ARITH_EXCEP_PLUS_O:
		fputs("[overflow on addition] ", stdout); break;
	case ARITH_EXCEP_PLUS_U:
		fputs("[underflow on addition] ", stdout); break;
	case ARITH_EXCEP_MINUS_O:
		fputs("[overflow on subtraction] ", stdout); break;
	case ARITH_EXCEP_MINUS_U:
		fputs("[underflow on subtraction] ", stdout); break;
	case ARITH_EXCEP_AND_T:
		fputs("[trap representation on bitwise and] ", stdout); break;
	case ARITH_EXCEP_XOR_T:
		fputs("[trap representation on bitwise xor] ", stdout); break;
	case ARITH_EXCEP_OR_T:
		fputs("[trap representation on bitwise or] ", stdout); break;
	case ARITH_EXCEP_LSH_W:
		fputs("[left shift by type width or more] ", stdout); break;
	case ARITH_EXCEP_LSH_C:
		fputs("[left shift by negative count] ", stdout); break;
	case ARITH_EXCEP_LSH_O:
		fputs("[overflow on left shift] ", stdout); break;
	case ARITH_EXCEP_LSH_U:
		fputs("[underflow on left shift] ", stdout); break;
	case ARITH_EXCEP_RSH_W:
		fputs("[right shift by type width or more] ", stdout); break;
	case ARITH_EXCEP_RSH_C:
		fputs("[right shift by negative count] ", stdout); break;
	case ARITH_EXCEP_RSH_N:
		fputs("[right shift of negative value] ", stdout); break;
	case ARITH_EXCEP_STAR_O:
		fputs("[overflow on multiplication] ", stdout); break;
	case ARITH_EXCEP_STAR_U:
		fputs("[underflow on multiplication] ", stdout); break;
	default:
		fprintf(stdout, "UNKNOWN WARNING TYPE: %d\n", type);
		exit(EXIT_FAILURE);
	}
}

void z_error(int type)
{
	switch (type) {
	case ARITH_EXCEP_SLASH_D:
		fputs("division by 0\n", stdout);
		break;
	case ARITH_EXCEP_SLASH_O:
		fputs("overflow on division\n", stdout);
		break;
	case ARITH_EXCEP_PCT_D:
		fputs("division by 0 on modulus operator\n", stdout);
		break;
	default:
		fprintf(stdout, "UNKNOWN ERROR TYPE: %d\n", type);
		exit(EXIT_FAILURE);
	}
	longjmp(jbuf, 1);
}

int main(void)
{

#define OPTRY_GEN(op, x, y, convx, convy, printz)   do { \
		printf("%s %s %s -> ", #x, #op, #y); \
		if (!setjmp(jbuf)) { \
			printz(zoinx_ ## op (convx(x), convy(y))); \
			putchar('\n'); \
		} \
	} while (0)

#define IDENT(x)  x

#define OPTRY_UU_U(op, x, y) \
	OPTRY_GEN(op, x, y, unsigned_to_uz, unsigned_to_uz, print_uz)

#define OPTRY_UI_U(op, x, y) \
	OPTRY_GEN(op, x, y, unsigned_to_uz, IDENT, print_uz)

#define OPTRY_UU_I(op, x, y) \
	OPTRY_GEN(op, x, y, unsigned_to_uz, unsigned_to_uz, print_int)

#define OPTRY_SS_S(op, x, y) \
	OPTRY_GEN(op, x, y, int_to_sz, int_to_sz, print_sz)

#define OPTRY_SI_S(op, x, y) \
	OPTRY_GEN(op, x, y, int_to_sz, IDENT, print_sz)

#define OPTRY_SS_I(op, x, y) \
	OPTRY_GEN(op, x, y, int_to_sz, int_to_sz, print_int)

	OPTRY_UU_U(u_plus, 3, 4);
	OPTRY_UU_U(u_plus, 1549587182, 1790478233);
	OPTRY_UU_U(u_minus, 1549587182, 1790478233);
	OPTRY_UU_U(u_minus, 1790478233, 1549587182);
	OPTRY_UU_U(u_star, 432429875, 347785487);
	OPTRY_UU_U(u_slash, 432429875, 34487);
	OPTRY_UU_U(u_pct, 432429875, 34487);
	OPTRY_UI_U(u_lsh, 1783, 19);
	OPTRY_UI_U(u_lsh, 1783, 20);
	OPTRY_UI_U(u_lsh, 1783, 21);
	OPTRY_UI_U(u_rsh, 475902857, 7);
	OPTRY_UI_U(u_rsh, 475902857, 17);
	OPTRY_UI_U(u_rsh, 475902857, 38);

	OPTRY_SS_S(s_plus, 3, 4);
	OPTRY_SS_S(s_plus, 1549587182, 1790478233);
	OPTRY_SS_S(s_plus, -1549587182, -1790478233);
	OPTRY_SS_S(s_minus, 1549587182, 1790478233);
	OPTRY_SS_S(s_minus, 1790478233, 1549587182);
	OPTRY_SS_S(s_minus, -1790478233, -1549587182);
	OPTRY_SS_S(s_minus, -1790478233, 1549587182);
	OPTRY_SS_S(s_star, 432429875, 347785487);
	OPTRY_SS_S(s_star, 432429875, -347785487);
	OPTRY_SS_S(s_slash, 432429875, 34487);
	OPTRY_SS_S(s_slash, -432429875, 34487);
	OPTRY_SS_S(s_slash, 432429875, -34487);
	OPTRY_SS_S(s_slash, -432429875, -34487);
	OPTRY_SS_S(s_slash, 432429875, 0);
	OPTRY_SS_S(s_slash, -2147483647 - 1, -1);
	OPTRY_SS_S(s_pct, 432429875, 34487);
	OPTRY_SS_S(s_pct, 432429875, 0);
	OPTRY_SI_S(s_lsh, -1, 10);
	OPTRY_SI_S(s_lsh, 1783, 19);
	OPTRY_SI_S(s_lsh, 1783, 20);
	OPTRY_SI_S(s_lsh, 1783, 21);
	OPTRY_SI_S(s_rsh, -1024, 8);
	OPTRY_SI_S(s_rsh, 475902857, 7);
	OPTRY_SI_S(s_rsh, 475902857, 17);

	return 0;
}
