/*
 * Integer arithmetic evaluation.
 *
 * (c) Thomas Pornin 2002
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. The name of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <limits.h>
#include "arith.h"

#define ARITH_OCTAL(x)   ((x) >= '0' && (x) <= '7')
#define ARITH_OVAL(x)    ((x) - '0')
#define ARITH_DECIM(x)   ((x) >= '0' && (x) <= '9')
#define ARITH_DVAL(x)    ((x) - '0')
#define ARITH_HEXAD(x)   (ARITH_DECIM(x) \
                         || (x) == 'a' || (x) == 'A' \
                         || (x) == 'b' || (x) == 'B' \
                         || (x) == 'c' || (x) == 'C' \
                         || (x) == 'd' || (x) == 'D' \
                         || (x) == 'e' || (x) == 'E' \
                         || (x) == 'f' || (x) == 'F')
#define ARITH_HVAL(x)    (ARITH_DECIM(x) ? ARITH_DVAL(x) \
                         : (x) == 'a' || (x) == 'A' ? 10 \
                         : (x) == 'b' || (x) == 'B' ? 11 \
                         : (x) == 'c' || (x) == 'C' ? 12 \
                         : (x) == 'd' || (x) == 'D' ? 13 \
                         : (x) == 'e' || (x) == 'E' ? 14 : 15)

#ifdef NATIVE_SIGNED
/* ====================================================================== */
/* Arithmetics with native types */
/* ====================================================================== */

/*
 * The following properties are imposed by the C standard:
 *
 * -- Arithmetics on the unsigned type should never overflow; every
 * result is reduced modulo some power of 2. The macro NATIVE_UNSIGNED_BITS
 * should have been defined to that specific exponent.
 *
 * -- The signed type should use either two's complement, one's complement
 * or a sign bit and a magnitude. There should be an integer N such that
 * the maximum signed value is (2^N)-1 and the minimum signed value is
 * either -(2^N) or -((2^N)-1). -(2^N) is possible only for two's complement.
 *
 * -- The maximum signed value is at most equal to the maximum unsigned
 * value.
 *
 * -- Trap representations can only be:
 *    ** In two's complement, 1 as sign bit and 0 for all value bits.
 *       This can happen only if the minimum signed value is -((2^N)-1).
 *    ** In one's complement, all bits set to 1.
 *    ** In mantissa + sign, sign bit to 1 and 0 for all value bits.
 * Unsigned values have no trap representation achievable with numerical
 * operators. Only signed values can have such representations, with
 * operators &, |, ^, ~, << and >>. If trap representations are possible,
 * such occurrences are reported as warnings.
 *
 * -- The operators +, -, * and << may overflow or underflow on signed
 * quantities, which is potentially an error. A warning is emitted.
 *
 * -- The operator >> yields an implementation-defined result on
 * signed negative quantities. Usually, the sign is extended, but this
 * is not guaranteed. A warning is emitted.
 *
 * -- The operators / and % used with a second operand of 0 cannot work.
 * An error is emitted when such a call is performed. Furthermore, in
 * two's complemement representation, with NATIVE_SIGNED_MIN == -(2^N)
 * for some N, the expression `NATIVE_SIGNED_MIN / (-1)' yields an
 * unrepresentable result, which is also an error.
 *
 *
 * For the value checks, we need to consider those different cases. So
 * we calculate the following macros:
 *   -- TWOS_COMPLEMENT: is 1 if representation is two's complement, 0
 *      otherwise.
 *   -- ONES_COMPLEMENT: is 1 if representation is one's complement, 0
 *      otherwise.
 *   -- SIGNED_IS_BIGGER: 1 if the maximum signed value is equal to the
 *      maximum unsigned value, 0 otherwise. NATIVE_SIGNED_MAX cannot
 *      exceed the maximum unsigned value. If SIGNED_IS_BIGGER is 0, then
 *      the maximum unsigned value is strictly superior to twice the
 *      value of NATIVE_SIGNED_MAX (e.g. 65535 to 32767).
 *   -- TRAP_REPRESENTATION: 1 if a trap representation is possible, 0
 *      otherwise. The only way trap representations are guaranteed
 *      impossible is when TWOS_COMPLEMENT is set, and NATIVE_SIGNED_MIN
 *      is equal to -NATIVE_SIGNED_MAX - 1.
 *
 * Those macros are calculated by some preprocessor directives. This
 * supposes that the implementation conforms to C99. Rules on preprocessing
 * were quite looser in C90, and it could be that an old compiler, used
 * for a cross-compiling task, does not get those right. Therefore, if
 * ARCH_DEFINED is defined prior to the inclusion of this file, those
 * four macros are supposed to be already defined. Otherwise they are
 * (re)defined. The macro ARCH_TRAP_DEFINED has the same meaning, but
 * is limited to the TRAP_REPRESENTATION macro (if ARCH_TRAP_DEFINED is
 * defined, the macro TRAP_REPRESENTATION is supposed to be already
 * defined; the three other macros are recalculated).
 *
 *
 * To sum up:
 * -- Whenever a division operator (/ or %) is invoked and would yield
 * an unrepresentable result, ARITH_ERROR() is invoked.
 * -- With ARITHMETIC_CHECKS undefined, ARITH_WARNING() is never invoked.
 * -- With ARITHMETIC_CHECKS defined:
 *    ** If ARCH_DEFINED is defined, the including context must provide
 *       the macros TWOS_COMPLEMENT, ONES_COMPLEMENT, SIGNED_IS_BIGGER
 *       and TRAP_REPRESENTATION.
 *    ** Otherwise, if ARCH_TRAP_DEFINED is defined, the including context
 *       must provide the macro TRAP_REPRESENTATION.
 *    The code then detects all operator invokations that would yield an
 *    overflow, underflow, trap representation, or any implementation
 *    defined result or undefined behaviour. The macro ARITH_WARNING() is
 *    invoked for each detection.
 * -- Trap representation detection code supposes that the operands are
 * _not_ trap representation.
 */

#ifndef ARCH_DEFINED

#undef TWOS_COMPLEMENT
#undef ONES_COMPLEMENT
#undef SIGNED_IS_BIGGER
#ifndef ARCH_TRAP_DEFINED
#undef TRAP_REPRESENTATION
#endif

#if (-1) & 3 == 3
/*
 * Two's complement.
 */
#define TWOS_COMPLEMENT         1
#define ONES_COMPLEMENT         0
#ifndef ARCH_TRAP_DEFINED
#if NATIVE_SIGNED_MIN < -NATIVE_SIGNED_MAX
#define TRAP_REPRESENTATION     0
#else
#define TRAP_REPRESENTATION     1
#endif
#endif

#elif (-1) & 3 == 2
/*
 * One's complement.
 */
#define TWOS_COMPLEMENT         0
#define ONES_COMPLEMENT         1
#ifndef ARCH_TRAP_DEFINED
#define TRAP_REPRESENTATION     1
#endif

#else
/*
 * Mantissa + sign.
 */
#define TWOS_COMPLEMENT         0
#define ONES_COMPLEMENT         0
#ifndef ARCH_TRAP_DEFINED
#define TRAP_REPRESENTATION     1
#endif

#endif

/*
 * Maximum native unsigned value. The first macro is for #if directives,
 * the second macro is for use as constant expression in C code.
 */
#define NATIVE_UNSIGNED_MAX     ((((1U << (NATIVE_UNSIGNED_BITS - 1)) - 1U) \
                                << 1) + 1U)
#define NATIVE_UNSIGNED_MAX_A   (((((arith_u)1 << (NATIVE_UNSIGNED_BITS - 1)) \
                                - (arith_u)1) << 1) + (arith_u)1)

#if NATIVE_SIGNED_MAX == NATIVE_UNSIGNED_MAX
#define SIGNED_IS_BIGGER        1
#else
#define SIGNED_IS_BIGGER        0
#endif

#endif

#undef NEGATIVE_IS_BIGGER
#if NATIVE_SIGNED_MIN < -NATIVE_SIGNED_MAX
#define NEGATIVE_IS_BIGGER      1
#else
#define NEGATIVE_IS_BIGGER      0
#endif

/* sanity check: we cannot have a trap representation if we have
   two's complement with NATIVE_SIGNED_MIN < -NATIVE_SIGNED_MAX */
#if TRAP_REPRESENTATION && NEGATIVE_IS_BIGGER
#error Impossible to get trap representations.
#endif

/* operations on the unsigned type */

ARITH_DECL_MONO_S_U(to_u) { return (arith_u)x; }
ARITH_DECL_MONO_I_U(fromint) { return (arith_u)x; }
ARITH_DECL_MONO_L_U(fromulong) { return (arith_u)x; }

ARITH_DECL_MONO_U_I(toint)
{
#if NATIVE_UNSIGNED_MAX > INT_MAX
	if (x > (arith_u)INT_MAX) return INT_MAX;
#endif
	return (int)x;
}

ARITH_DECL_MONO_U_L(toulong)
{
#if NATIVE_UNSIGNED_MAX > LONG_MAX
	if (x > (arith_u)LONG_MAX) return LONG_MAX;
#endif
	return (long)x;
}

ARITH_DECL_MONO_U_U(neg) { return -x; }
ARITH_DECL_MONO_U_U(not) { return ~x; }
ARITH_DECL_MONO_U_I(lnot) { return !x; }
ARITH_DECL_MONO_U_I(lval) { return x != 0; }

ARITH_DECL_BI_UU_U(plus) { return x + y; }
ARITH_DECL_BI_UU_U(minus) { return x - y; }
ARITH_DECL_BI_UU_I(lt) { return x < y; }
ARITH_DECL_BI_UU_I(leq) { return x <= y; }
ARITH_DECL_BI_UU_I(gt) { return x > y; }
ARITH_DECL_BI_UU_I(geq) { return x >= y; }
ARITH_DECL_BI_UU_I(same) { return x == y; }
ARITH_DECL_BI_UU_I(neq) { return x != y; }
ARITH_DECL_BI_UU_U(and) { return x & y; }
ARITH_DECL_BI_UU_U(xor) { return x ^ y; }
ARITH_DECL_BI_UU_U(or) { return x | y; }
ARITH_DECL_BI_UU_U(star) { return x * y; }

ARITH_DECL_BI_UI_U(lsh)
{
#ifdef ARITHMETIC_CHECKS
	if (y >= NATIVE_UNSIGNED_BITS)
		ARITH_WARNING(ARITH_EXCEP_LSH_W);
	else if (y < 0)
		ARITH_WARNING(ARITH_EXCEP_LSH_C);
#endif
	return x << y;
}

ARITH_DECL_BI_UI_U(rsh)
{
#ifdef ARITHMETIC_CHECKS
	if (y >= NATIVE_UNSIGNED_BITS)
		ARITH_WARNING(ARITH_EXCEP_RSH_W);
	else if (y < 0)
		ARITH_WARNING(ARITH_EXCEP_RSH_C);
#endif
	return x >> y;
}

ARITH_DECL_BI_UU_U(slash)
{
	if (y == 0) ARITH_ERROR(ARITH_EXCEP_SLASH_D);
	return x / y;
}

ARITH_DECL_BI_UU_U(pct)
{
	if (y == 0) ARITH_ERROR(ARITH_EXCEP_PCT_D);
	return x % y;
}

/* operations on the signed type */

ARITH_DECL_MONO_U_S(to_s)
{
#ifdef ARITHMETIC_CHECKS
#if !SIGNED_IS_BIGGER
	if (x > (arith_u)NATIVE_SIGNED_MAX)
		ARITH_WARNING(ARITH_EXCEP_CONV_O);
#endif
#endif
	return (arith_s)x;
}

ARITH_DECL_MONO_I_S(fromint) { return (arith_s)x; }
ARITH_DECL_MONO_L_S(fromlong) { return (arith_s)x; }

ARITH_DECL_MONO_S_I(toint)
{
#if NATIVE_SIGNED_MIN < INT_MIN
	if (x < (arith_s)INT_MIN) return INT_MIN;
#endif
#if NATIVE_SIGNED_MAX > INT_MAX
	if (x > (arith_s)INT_MAX) return INT_MAX;
#endif
	return (int)x;
}

ARITH_DECL_MONO_S_L(tolong)
{
#if NATIVE_SIGNED_MIN < LONG_MIN
	if (x < (arith_s)LONG_MIN) return LONG_MIN;
#endif
#if NATIVE_SIGNED_MAX > LONG_MAX
	if (x > (arith_s)LONG_MAX) return LONG_MAX;
#endif
	return (long)x;
}

ARITH_DECL_MONO_S_S(neg)
{
#ifdef ARITHMETIC_CHECKS
#if NEGATIVE_IS_BIGGER
	if (x == NATIVE_SIGNED_MIN)
		ARITH_WARNING(ARITH_EXCEP_NEG_O);
#endif
#endif
	return -x;
}

ARITH_DECL_MONO_S_S(not)
{
#ifdef ARITHMETIC_CHECKS
#if TRAP_REPRESENTATION
	if (
#if TWOS_COMPLEMENT
		(x == NATIVE_SIGNED_MAX)
#elif ONES_COMPLEMENT
		(x == 0)
#else
		(x == NATIVE_SIGNED_MAX)
#endif
		) ARITH_WARNING(ARITH_EXCEP_NOT_T);
#endif
#endif
	return ~x;
}

ARITH_DECL_MONO_S_I(lnot) { return !x; }
ARITH_DECL_MONO_S_I(lval) { return x != 0; }

/*
 * Addition of signed values:
 * -- overflows occur only when both operands are strictly positive
 * -- underflows occur only when both operands are strictly negative
 * -- overflow check (both operands > 0):
 *    ** if SIGNED_IS_BIGGER == 1, overflows are kept as such in the
 *       unsigned world (if the signed addition overflows, so does the
 *       unsigned, and vice versa)
 *    ** if SIGNED_IS_BIGGER == 0, no overflow can happen in the unsigned
 *       world
 * -- underflow check (both operands < 0):
 *    ** if NEGATIVE_IS_BIGGER == 1 (must be two's complement)
 *       ++ we have a guaranteed underflow if one of the operand is equal
 *          to NATIVE_SIGNED_MIN; otherwise, -x and -y are valid integers,
 *          and we cast them into the unsigned world
 *       ++ if SIGNED_IS_BIGGER == 1, underflows become unsigned overflows
 *          with a non-zero result
 *       ++ if SIGNED_IS_BIGGER == 0, no overflow happens in the unsigned
 *          world; we use the fact that -NATIVE_SIGNED_MIN is then
 *          exaxctly 1 more than NATIVE_SIGNED_MAX
 *    ** if NEGATIVE_IS_BIGGER == 0, underflow check is identical to
 *       overflow check on (signed) -x and -y.
 */
ARITH_DECL_BI_SS_S(plus)
{
#ifdef ARITHMETIC_CHECKS
	if (x > 0 && y > 0 && (
#if SIGNED_IS_BIGGER
		((arith_u)((arith_u)x + (arith_u)y) < (arith_u)x)
#else
		(((arith_u)x + (arith_u)y) > (arith_u)NATIVE_SIGNED_MAX)
#endif
		)) ARITH_WARNING(ARITH_EXCEP_PLUS_O);
	else if (x < 0 && y < 0 && (
#if NEGATIVE_IS_BIGGER
		(x == NATIVE_SIGNED_MIN || y == NATIVE_SIGNED_MIN) ||
#if SIGNED_IS_BIGGER
		(((arith_u)(-x) + (arith_u)(-y) != 0)
			&& (arith_u)((arith_u)(-x) + (arith_u)(-y))
			< (arith_u)(-x))
#else
		(((arith_u)(-x) + (arith_u)(-y))
			> ((arith_u)1 + (arith_u)NATIVE_SIGNED_MAX))
#endif
#else
#if SIGNED_IS_BIGGER
		((arith_u)((arith_u)(-x) + (arith_u)(-y)) < (arith_u)(-x))
#else
		(((arith_u)(-x) + (arith_u)(-y))
			> (arith_u)NATIVE_SIGNED_MAX)
#endif
#endif
		)) ARITH_WARNING(ARITH_EXCEP_PLUS_U);
#endif
	return x + y;
}

/*
 * Subtraction of signed values:
 * -- overflow: only if x > 0 and y < 0
 *    ** if NEGATIVE_IS_BIGGER == 1 (must be two's complement) and
 *       y == NATIVE_SIGNED_MIN then overflow
 *    ** otherwise, cast x and -y to unsigned, then add and check
 *       for overflows
 * -- underflow: only if x < 0 and y > 0
 *    ** if NEGATIVE_IS_BIGGER == 1 (must be two's complement):
 *       ++ if x == NATIVE_SIGNED_MIN then underflow
 *       ++ cast -x and y to unsigned, then add. If SIGNED_IS_BIGGER == 0,
 *          just check. Otherwise, check for overflow with non-zero result.
 *    ** if NEGATIVE_IS_BIGGER == 0: cast -x and y to unsigned, then
 *       add. Overflow check as in addition.
 */
ARITH_DECL_BI_SS_S(minus)
{
#ifdef ARITHMETIC_CHECKS
	if (x > 0 && y < 0 && (
#if NEGATIVE_IS_BIGGER
	(y == NATIVE_SIGNED_MIN) ||
#endif
#if SIGNED_IS_BIGGER
	((arith_u)((arith_u)x + (arith_u)(-y)) < (arith_u)x)
#else
	(((arith_u)x + (arith_u)(-y)) > (arith_u)NATIVE_SIGNED_MAX)
#endif
	)) ARITH_WARNING(ARITH_EXCEP_MINUS_O);
	else if (x < 0 && y > 0 && (
#if NEGATIVE_IS_BIGGER
	(x == NATIVE_SIGNED_MIN) ||
#if SIGNED_IS_BIGGER
	((((arith_u)(-x) + (arith_u)y) != 0) &&
		((arith_u)((arith_u)(-x) + (arith_u)y) < (arith_u)(-x)))
#else
	(((arith_u)(-x) + (arith_u)y) >
		((arith_u)1 + (arith_u)NATIVE_SIGNED_MAX))
#endif
#else
#if SIGNED_IS_BIGGER
	((arith_u)((arith_u)(-x) + (arith_u)y) < (arith_u)(-x))
#else
	(((arith_u)(-x) + (arith_u)y) > (arith_u)NATIVE_SIGNED_MAX)
#endif
#endif
	)) ARITH_WARNING(ARITH_EXCEP_MINUS_U);
#endif
	return x - y;
}

ARITH_DECL_BI_SS_I(lt) { return x < y; }
ARITH_DECL_BI_SS_I(leq) { return x <= y; }
ARITH_DECL_BI_SS_I(gt) { return x > y; }
ARITH_DECL_BI_SS_I(geq) { return x >= y; }
ARITH_DECL_BI_SS_I(same) { return x == y; }
ARITH_DECL_BI_SS_I(neq) { return x != y; }

/*
 * Provided neither x nor y is a trap representation:
 * -- one's complement: impossible to get a trap representation
 * -- two's complement and sign + mantissa: trap representation if and
 * only if x and y are strictly negative and (-x) & (-y) == 0
 * (in two's complement, -x is safe because overflow would occur only
 * if x was already a trap representation).
 */
ARITH_DECL_BI_SS_S(and)
{
#ifdef ARITHMETIC_CHECKS
#if TRAP_REPRESENTATION && !ONES_COMPLEMENT
	if (x < 0 && y < 0 && ((-x) & (-y)) == 0)
		ARITH_WARNING(ARITH_EXCEP_AND_T);
#endif
#endif
	return x & y;
}

/*
 * Provided neither x nor y is a trap representation:
 * -- two's complement: trap if and only if x != NATIVE_SIGNED_MAX && ~x == y
 * -- one's complement: trap if and only if x != 0 && ~x == y
 * -- mantissa + sign: trap if and only if x != 0 && -x == y
 */
ARITH_DECL_BI_SS_S(xor)
{
#ifdef ARITHMETIC_CHECKS
#if TRAP_REPRESENTATION
	if (
#if TWOS_COMPLEMENT
	(x != NATIVE_SIGNED_MAX && ~x == y)
#elif ONES_COMPLEMENT
	(x != 0 && ~x == y)
#else
	(x != 0 && -x == y)
#endif
		) ARITH_WARNING(ARITH_EXCEP_XOR_T);
#endif
#endif
	return x ^ y;
}

/*
 * Provided neither x nor y is a trap representation:
 * -- two's complement: impossible to trap
 * -- one's complement: trap if and only if x != 0 && y != 0 && (~x & ~y) == 0
 * -- mantissa + sign: impossible to trap
 */
ARITH_DECL_BI_SS_S(or)
{
#ifdef ARITHMETIC_CHECKS
#if TRAP_REPRESENTATION
#if ONES_COMPLEMENT
	if (x != 0 && y != 0 && (~x & ~y) == 0)
		ARITH_WARNING(ARITH_EXCEP_OR_T);
#endif
#endif
#endif
	return x | y;
}

/*
 * Left-shifting by a negative or greater than type width count is
 * forbidden. Left-shifting a negative value is forbidden (underflow).
 * Left-shifting a positive value can trigger an overflow. We check it
 * by casting into the unsigned world and simulating a truncation.
 *
 * If SIGNED_IS_BIGGER is set, then the signed type width is 1 more
 * than the unsigned type width (the sign bit is included in the width);
 * otherwise, if W is the signed type width, 1U << (W-1) is equal to
 * NATIVE_SIGNED_MAX + 1.
 */
ARITH_DECL_BI_SI_S(lsh)
{
#ifdef ARITHMETIC_CHECKS
	if (y < 0) ARITH_WARNING(ARITH_EXCEP_LSH_C);
	else if (
#if SIGNED_IS_BIGGER
		y > NATIVE_UNSIGNED_BITS
#else
		y >= NATIVE_UNSIGNED_BITS
		|| (y > 0 && (((arith_u)1 << (y - 1))
			> (arith_u)NATIVE_SIGNED_MAX))
#endif
		) ARITH_WARNING(ARITH_EXCEP_LSH_W);
	else if (x < 0) ARITH_WARNING(ARITH_EXCEP_LSH_U);
	else if (x > 0 && ((((arith_u)x << y) & NATIVE_SIGNED_MAX) >> y)
		!= (arith_u)x) ARITH_WARNING(ARITH_EXCEP_LSH_O);
#endif
	return x << y;
}

/*
 * Right-shifting is handled as left-shifting, except that the problem
 * is somehow simpler: there is no possible overflow or underflow. Only
 * right-shifting a negative value yields an implementation defined
 * result (_not_ an undefined behaviour).
 */
ARITH_DECL_BI_SI_S(rsh)
{
#ifdef ARITHMETIC_CHECKS
	if (y < 0) ARITH_WARNING(ARITH_EXCEP_RSH_C);
	else if (
#if SIGNED_IS_BIGGER
		y > NATIVE_UNSIGNED_BITS
#else
		y >= NATIVE_UNSIGNED_BITS
		|| (y > 0 && (((arith_u)1 << (y - 1))
			> (arith_u)NATIVE_SIGNED_MAX))
#endif
		) ARITH_WARNING(ARITH_EXCEP_RSH_W);
	else if (x < 0) ARITH_WARNING(ARITH_EXCEP_RSH_N);
#endif
	return x >> y;
}

/*
 * Overflow can happen only if both operands have the same sign.
 * Underflow can happen only if both operands have opposite signs.
 *
 * Overflow checking: this is done quite inefficiently by performing
 * a division on the result and check if it matches the initial operand.
 */
ARITH_DECL_BI_SS_S(star)
{
#ifdef ARITHMETIC_CHECKS
	if (x == 0 || y == 0) return 0;
	if (x > 0 && y > 0) {
		if ((((arith_u)x * (arith_u)y) & (arith_u)NATIVE_SIGNED_MAX)
			/ (arith_u)y != (arith_u)x)
			ARITH_WARNING(ARITH_EXCEP_STAR_O);
	} else if (x < 0 && y < 0) {
		if (
#if NEGATIVE_IS_BIGGER
			(x == NATIVE_SIGNED_MIN || y == NATIVE_SIGNED_MIN) ||
#endif
			(((arith_u)(-x) * (arith_u)(-y))
			& (arith_u)NATIVE_SIGNED_MAX) / (arith_u)(-y)
			!= (arith_u)(-x))
			ARITH_WARNING(ARITH_EXCEP_STAR_O);
	} else if (x > 0 && y < 0) {
		if ((arith_u)x > (arith_u)1 && (
#if NEGATIVE_IS_BIGGER
		y == NATIVE_SIGNED_MIN ||
#endif
		(((arith_u)x * (arith_u)(-y)) & (arith_u)NATIVE_SIGNED_MAX)
		/ (arith_u)(-y) != (arith_u)x))
		ARITH_WARNING(ARITH_EXCEP_STAR_U);
	} else {
		if ((arith_u)y > (arith_u)1 && (
#if NEGATIVE_IS_BIGGER
		x == NATIVE_SIGNED_MIN ||
#endif
		(((arith_u)y * (arith_u)(-x)) & (arith_u)NATIVE_SIGNED_MAX)
		/ (arith_u)(-x) != (arith_u)y))
		ARITH_WARNING(ARITH_EXCEP_STAR_U);
	}
#endif
	return x * y;
}

/*
 * Division by 0 is an error. The only other possible problem is an
 * overflow of the result. Such an overflow can only happen in two's
 * complement representation, when NEGATIVE_IS_BIGGER is set, and
 * one attempts to divide NATIVE_SIGNED_MIN by -1: the result is then
 * -NATIVE_SIGNED_MIN, which is not representable by the type. This is
 * considered as an error, not a warning, because it actually triggers
 * an exception on modern Pentium-based PC.
 */
ARITH_DECL_BI_SS_S(slash)
{
	if (y == 0) ARITH_ERROR(ARITH_EXCEP_SLASH_D);
#if NEGATIVE_IS_BIGGER
	else if (x == NATIVE_SIGNED_MIN && y == (arith_s)(-1))
		ARITH_ERROR(ARITH_EXCEP_SLASH_O);
#endif
	return x / y;
}

/*
 * Only division by 0 needs to be checked.
 */
ARITH_DECL_BI_SS_S(pct)
{
	if (y == 0) ARITH_ERROR(ARITH_EXCEP_PCT_D);
	return x % y;
}

ARITH_DECL_MONO_ST_US(octconst)
{
	arith_u z = 0;

	for (; ARITH_OCTAL(*c); c ++) {
		arith_u w = ARITH_OVAL(*c);
		if (z > (NATIVE_UNSIGNED_MAX_A / 8))
			ARITH_ERROR(ARITH_EXCEP_CONST_O);
		z *= 8;
#if 0
/* obsolete */
/* NATIVE_UNSIGNED_MAX_A is 2^N - 1, 0 <= w <= 7 and 8 divides z */
		if (z > (NATIVE_UNSIGNED_MAX_A - w))
			ARITH_ERROR(ARITH_EXCEP_CONST_O);
#endif
		z += w;
	}
	*ru = z;
#if SIGNED_IS_BIGGER
	*rs = z;
	*sp = 1;
#else
	if (z > NATIVE_SIGNED_MAX) {
		*sp = 0;
	} else {
		*rs = z;
		*sp = 1;
	}
#endif
	return c;
}

ARITH_DECL_MONO_ST_US(decconst)
{
	arith_u z = 0;

	for (; ARITH_DECIM(*c); c ++) {
		arith_u w = ARITH_DVAL(*c);
		if (z > (NATIVE_UNSIGNED_MAX_A / 10))
			ARITH_ERROR(ARITH_EXCEP_CONST_O);
		z *= 10;
		if (z > (NATIVE_UNSIGNED_MAX_A - w))
			ARITH_ERROR(ARITH_EXCEP_CONST_O);
		z += w;
	}
	*ru = z;
#if SIGNED_IS_BIGGER
	*rs = z;
	*sp = 1;
#else
	if (z > NATIVE_SIGNED_MAX) {
		*sp = 0;
	} else {
		*rs = z;
		*sp = 1;
	}
#endif
	return c;
}

ARITH_DECL_MONO_ST_US(hexconst)
{
	arith_u z = 0;

	for (; ARITH_HEXAD(*c); c ++) {
		arith_u w = ARITH_HVAL(*c);
		if (z > (NATIVE_UNSIGNED_MAX_A / 16))
			ARITH_ERROR(ARITH_EXCEP_CONST_O);
		z *= 16;
#if 0
/* obsolete */
/* NATIVE_UNSIGNED_MAX_A is 2^N - 1, 0 <= w <= 15 and 16 divides z */
		if (z > (NATIVE_UNSIGNED_MAX_A - w))
			ARITH_ERROR(ARITH_EXCEP_CONST_O);
#endif
		z += w;
	}
	*ru = z;
#if SIGNED_IS_BIGGER
	*rs = z;
	*sp = 1;
#else
	if (z > NATIVE_SIGNED_MAX) {
		*sp = 0;
	} else {
		*rs = z;
		*sp = 1;
	}
#endif
	return c;
}

#else
/* ====================================================================== */
/* Arithmetics with a simple simulated type */
/* ====================================================================== */

/*
 * We simulate a type with the following characteristics:
 * -- the signed type width is equal to the unsigned type width (which
 * means that there is one less value bit in the signed type);
 * -- the signed type uses two's complement representation;
 * -- there is no trap representation;
 * -- overflows and underflows are truncated (but a warning is emitted
 * if ARITHMETIC_CHECKS is defined);
 * -- overflow on integer division is still an error;
 * -- right-shifting of a negative value extends the sign;
 * -- the shift count value is first cast to unsigned, then reduced modulo
 * the type size.
 *
 * These characteristics follow what is usually found on modern
 * architectures.
 *
 * The maximum emulated type size is twice the size of the unsigned native
 * type which is used to emulate the type.
 */

#undef SIMUL_ONE_TMP
#undef SIMUL_MSW_TMP1
#undef SIMUL_MSW_MASK
#undef SIMUL_LSW_TMP1
#undef SIMUL_LSW_MASK

#define SIMUL_ONE_TMP     ((SIMUL_ARITH_SUBTYPE)1)
#define SIMUL_MSW_TMP1    (SIMUL_ONE_TMP << (SIMUL_MSW_WIDTH - 1))
#define SIMUL_MSW_MASK    (SIMUL_MSW_TMP1 | (SIMUL_MSW_TMP1 - SIMUL_ONE_TMP))
#define SIMUL_LSW_TMP1    (SIMUL_ONE_TMP << (SIMUL_LSW_WIDTH - 1))
#define SIMUL_LSW_MASK    (SIMUL_LSW_TMP1 | (SIMUL_LSW_TMP1 - SIMUL_ONE_TMP))

#undef TMSW
#undef TLSW

#define TMSW(x)           ((x) & SIMUL_MSW_MASK)
#define TLSW(x)           ((x) & SIMUL_LSW_MASK)

#undef SIMUL_ZERO
#undef SIMUL_ONE

#define SIMUL_ZERO        arith_strc(ARITH_TYPENAME, _zero)
#define SIMUL_ONE         arith_strc(ARITH_TYPENAME, _one)

static arith_u SIMUL_ZERO = { 0, 0 };
static arith_u SIMUL_ONE = { 0, 1 };

/*
 * We use the fact that both the signed and unsigned type are the same
 * structure. The difference between the signed and the unsigned type
 * is a type information, and, as such, is considered compile-time and
 * not maintained in the value structure itself. This is a job for
 * the programmer / compiler.
 */
ARITH_DECL_MONO_S_U(to_u) { return x; }

ARITH_DECL_MONO_I_U(fromint)
{
	arith_u z;

	if (x < 0) return arith_op_u(neg)(arith_op_u(fromint)(-x));
	/*
	 * This code works because types smaller than int are promoted
	 * by the C compiler before evaluating the >> operator.
	 */
	z.msw = TMSW(((SIMUL_ARITH_SUBTYPE)x >> (SIMUL_LSW_WIDTH - 1)) >> 1);
	z.lsw = TLSW((SIMUL_ARITH_SUBTYPE)x);
	return z;
}

ARITH_DECL_MONO_L_U(fromulong)
{
	arith_u z;

#if (ULONG_MAX >> (SIMUL_LSW_WIDTH - 1)) >> 1 == 0
	z.msw = 0;
	z.lsw = x;
#else
	z.msw = TMSW(x >> SIMUL_LSW_WIDTH);
	z.lsw = TLSW((SIMUL_ARITH_SUBTYPE)x);
#endif
	return z;
}

ARITH_DECL_MONO_U_I(toint)
{
#if ((INT_MAX >> (SIMUL_LSW_WIDTH - 1)) >> 1) == 0
	if (x.msw != 0 || x.lsw > (SIMUL_ARITH_SUBTYPE)INT_MAX)
		return INT_MAX;
	return (int)x.lsw;
#else
#if (INT_MAX >> (SIMUL_SUBTYPE_BITS - 1)) == 0
	if (x.msw > (SIMUL_ARITH_SUBTYPE)(INT_MAX >> SIMUL_LSW_WIDTH))
		return INT_MAX;
#endif
	return ((int)x.msw << SIMUL_LSW_WIDTH) | (int)x.lsw;
#endif
}

ARITH_DECL_MONO_U_L(toulong)
{
#if ((ULONG_MAX >> (SIMUL_LSW_WIDTH - 1)) >> 1) == 0
	if (x.msw != 0 || x.lsw > (SIMUL_ARITH_SUBTYPE)ULONG_MAX)
		return ULONG_MAX;
	return (unsigned long)x.lsw;
#else
#if (ULONG_MAX >> (SIMUL_SUBTYPE_BITS - 1)) == 0
	if (x.msw > (SIMUL_ARITH_SUBTYPE)(ULONG_MAX >> SIMUL_LSW_WIDTH))
		return ULONG_MAX;
#endif
	return ((unsigned long)x.msw << SIMUL_LSW_WIDTH) | (unsigned long)x.lsw;
#endif
}

ARITH_DECL_MONO_U_U(neg)
{
	x = arith_op_u(not)(x);
	return arith_op_u(plus)(x, SIMUL_ONE);
}

ARITH_DECL_MONO_U_U(not)
{
	x.msw = TMSW(~x.msw);
	x.lsw = TLSW(~x.lsw);
	return x;
}

ARITH_DECL_MONO_U_I(lnot)
{
	return x.msw == 0 && x.lsw == 0;
}

ARITH_DECL_MONO_U_I(lval)
{
	return x.msw != 0 || x.lsw != 0;
}

ARITH_DECL_BI_UU_U(plus)
{
	x.lsw = TLSW(x.lsw + y.lsw);
	x.msw = TMSW(x.msw + y.msw);
	if (x.lsw < y.lsw) x.msw = TMSW(x.msw + 1);
	return x;
}

ARITH_DECL_BI_UU_U(minus)
{
	return arith_op_u(plus)(x, arith_op_u(neg)(y));
}

ARITH_DECL_BI_UI_U(lsh)
{
	if (y == 0) return x;
#ifdef ARITHMETIC_CHECKS
	if (y < 0) ARITH_WARNING(ARITH_EXCEP_LSH_C);
	else if (y >= SIMUL_NUMBITS) ARITH_WARNING(ARITH_EXCEP_LSH_W);
#endif
	y = (unsigned)y % SIMUL_NUMBITS;
	if (y >= SIMUL_LSW_WIDTH) {
		/*
		 * We use here the fact that the LSW size is always
		 * equal to or greater than the MSW size.
		 */
		x.msw = TMSW(x.lsw << (y - SIMUL_LSW_WIDTH));
		x.lsw = 0;
		return x;
	}
	x.msw = TMSW((x.msw << y) | (x.lsw >> (SIMUL_LSW_WIDTH - y)));
	x.lsw = TLSW(x.lsw << y);
	return x;
}

ARITH_DECL_BI_UI_U(rsh)
{
#ifdef ARITHMETIC_CHECKS
	if (y < 0) ARITH_WARNING(ARITH_EXCEP_RSH_C);
	else if (y >= SIMUL_NUMBITS) ARITH_WARNING(ARITH_EXCEP_RSH_W);
#endif
	y = (unsigned)y % SIMUL_NUMBITS;
	if (y >= SIMUL_LSW_WIDTH) {
		x.lsw = x.msw >> (y - SIMUL_LSW_WIDTH);
		x.msw = 0;
		return x;
	}
	x.lsw = TLSW((x.lsw >> y) | (x.msw << (SIMUL_LSW_WIDTH - y)));
	x.msw >>= y;
	return x;
}

ARITH_DECL_BI_UU_I(lt)
{
	return x.msw < y.msw || (x.msw == y.msw && x.lsw < y.lsw);
}

ARITH_DECL_BI_UU_I(leq)
{
	return x.msw < y.msw || (x.msw == y.msw && x.lsw <= y.lsw);
}

ARITH_DECL_BI_UU_I(gt)
{
	return arith_op_u(lt)(y, x);
}

ARITH_DECL_BI_UU_I(geq)
{
	return arith_op_u(leq)(y, x);
}

ARITH_DECL_BI_UU_I(same)
{
	return x.msw == y.msw && x.lsw == y.lsw;
}

ARITH_DECL_BI_UU_I(neq)
{
	return !arith_op_u(same)(x, y);
}

ARITH_DECL_BI_UU_U(and)
{
	x.msw &= y.msw;
	x.lsw &= y.lsw;
	return x;
}

ARITH_DECL_BI_UU_U(xor)
{
	x.msw ^= y.msw;
	x.lsw ^= y.lsw;
	return x;
}

ARITH_DECL_BI_UU_U(or)
{
	x.msw |= y.msw;
	x.lsw |= y.lsw;
	return x;
}

#undef SIMUL_LSW_ODDLEN
#undef SIMUL_LSW_HALFLEN
#undef SIMUL_LSW_HALFMASK

#define SIMUL_LSW_ODDLEN    (SIMUL_LSW_WIDTH & 1)
#define SIMUL_LSW_HALFLEN   (SIMUL_LSW_WIDTH / 2)
#define SIMUL_LSW_HALFMASK  (~(~(SIMUL_ARITH_SUBTYPE)0 << SIMUL_LSW_HALFLEN))

ARITH_DECL_BI_UU_U(star)
{
	arith_u z;
	SIMUL_ARITH_SUBTYPE a = x.lsw, b = y.lsw, t00, t01, t10, t11, c = 0, t;
#if SIMUL_LSW_ODDLEN
	SIMUL_ARITH_SUBTYPE bms = b & (SIMUL_ONE_TMP << (SIMUL_LSW_WIDTH - 1));

	b &= ~(SIMUL_ONE_TMP << (SIMUL_LSW_WIDTH - 1));
#endif

	t00 = (a & SIMUL_LSW_HALFMASK) * (b & SIMUL_LSW_HALFMASK);
	t01 = (a & SIMUL_LSW_HALFMASK) * (b >> SIMUL_LSW_HALFLEN);
	t10 = (a >> SIMUL_LSW_HALFLEN) * (b & SIMUL_LSW_HALFMASK);
	t11 = (a >> SIMUL_LSW_HALFLEN) * (b >> SIMUL_LSW_HALFLEN);
	t = z.lsw = t00;
	z.lsw = TLSW(z.lsw + (t01 << SIMUL_LSW_HALFLEN));
	if (t > z.lsw) c ++;
	t = z.lsw;
	z.lsw = TLSW(z.lsw + (t10 << SIMUL_LSW_HALFLEN));
	if (t > z.lsw) c ++;
#if SIMUL_LSW_ODDLEN
	t = z.lsw;
	z.lsw = TLSW(z.lsw + (t11 << (2 * SIMUL_LSW_HALFLEN)));
	if (t > z.lsw) c ++;
	if (bms && (a & SIMUL_ONE_TMP)) {
		t = z.lsw;
		z.lsw = TLSW(z.lsw + b);
		if (t > z.lsw) c ++;
	}
#endif
	z.msw = TMSW(x.lsw * y.msw + x.msw * y.lsw + c
		+ (t01 >> (SIMUL_LSW_WIDTH - SIMUL_LSW_HALFLEN))
		+ (t10 >> (SIMUL_LSW_WIDTH - SIMUL_LSW_HALFLEN))
		+ (t11 >> (SIMUL_LSW_WIDTH - (2 * SIMUL_LSW_HALFLEN))));
	return z;
}

/*
 * This function calculates the unsigned integer division, yielding
 * both quotient and remainder. The divider (y) MUST be non-zero.
 */
static void arith_op_u(udiv)(arith_u x, arith_u y, arith_u *q, arith_u *r)
{
	int i, j;
	arith_u a;

	*q = SIMUL_ZERO;
	for (i = SIMUL_NUMBITS - 1; i >= 0; i --) {
		if (i >= (int)SIMUL_LSW_WIDTH
			&& (y.msw & (SIMUL_ONE_TMP << (i - SIMUL_LSW_WIDTH))))
			break;
		if (i < (int)SIMUL_LSW_WIDTH && (y.lsw & (SIMUL_ONE_TMP << i)))
			break;
	}
	a = arith_op_u(lsh)(y, SIMUL_NUMBITS - 1 - i);
	for (j = SIMUL_NUMBITS - 1 - i; j >= SIMUL_LSW_WIDTH; j --) {
		if (arith_op_u(leq)(a, x)) {
			x = arith_op_u(minus)(x, a);
			q->msw |= SIMUL_ONE_TMP << (j - SIMUL_LSW_WIDTH);
		}
		a = arith_op_u(rsh)(a, 1);
	}
	for (; j >= 0; j --) {
		if (arith_op_u(leq)(a, x)) {
			x = arith_op_u(minus)(x, a);
			q->lsw |= SIMUL_ONE_TMP << j;
		}
		a = arith_op_u(rsh)(a, 1);
	}
	*r = x;
}

ARITH_DECL_BI_UU_U(slash)
{
	arith_u q, r;

	if (arith_op_u(same)(y, SIMUL_ZERO))
		ARITH_ERROR(ARITH_EXCEP_SLASH_D);
	arith_op_u(udiv)(x, y, &q, &r);
	return q;
}

ARITH_DECL_BI_UU_U(pct)
{
	arith_u q, r;

	if (arith_op_u(same)(y, SIMUL_ZERO))
		ARITH_ERROR(ARITH_EXCEP_PCT_D);
	arith_op_u(udiv)(x, y, &q, &r);
	return r;
}

#undef SIMUL_TRAP
#undef SIMUL_TRAPL
#define SIMUL_TRAP   (SIMUL_ONE_TMP << (SIMUL_MSW_WIDTH - 1))
#define SIMUL_TRAPL  (SIMUL_ONE_TMP << (SIMUL_LSW_WIDTH - 1))

ARITH_DECL_MONO_U_S(to_s)
{
#ifdef ARITHMETIC_CHECKS
	if (x.msw & SIMUL_TRAP) ARITH_WARNING(ARITH_EXCEP_CONV_O);
#endif
	return x;
}

ARITH_DECL_MONO_I_S(fromint) { return arith_op_u(fromint)(x); }
ARITH_DECL_MONO_L_S(fromlong)
{
	if (x < 0) return arith_op_u(neg)(
		arith_op_u(fromulong)((unsigned long)(-x)));
	return arith_op_u(fromulong)((unsigned long)x);
}

ARITH_DECL_MONO_S_I(toint)
{
	if (x.msw & SIMUL_TRAP) return -arith_op_u(toint)(arith_op_u(neg)(x));
	return arith_op_u(toint)(x);
}

ARITH_DECL_MONO_S_L(tolong)
{
	if (x.msw & SIMUL_TRAP)
		return -(long)arith_op_u(toulong)(arith_op_u(neg)(x));
	return (long)arith_op_u(toulong)(x);
}

ARITH_DECL_MONO_S_S(neg)
{
#ifdef ARITHMETIC_CHECKS
	if (x.lsw == 0 && x.msw == SIMUL_TRAP)
		ARITH_WARNING(ARITH_EXCEP_NEG_O);
#endif
	return arith_op_u(neg)(x);
}

ARITH_DECL_MONO_S_S(not) { return arith_op_u(not)(x); }
ARITH_DECL_MONO_S_I(lnot) { return arith_op_u(lnot)(x); }
ARITH_DECL_MONO_S_I(lval) { return arith_op_u(lval)(x); }

ARITH_DECL_BI_SS_S(plus)
{
	arith_u z = arith_op_u(plus)(x, y);

#ifdef ARITHMETIC_CHECKS
	if (x.msw & y.msw & ~z.msw & SIMUL_TRAP)
		ARITH_WARNING(ARITH_EXCEP_PLUS_U);
	else if (~x.msw & ~y.msw & z.msw & SIMUL_TRAP)
		ARITH_WARNING(ARITH_EXCEP_PLUS_O);
#endif
	return z;
}

ARITH_DECL_BI_SS_S(minus)
{
	arith_s z = arith_op_u(minus)(x, y);

#ifdef ARITHMETIC_CHECKS
	if (x.msw & ~y.msw & ~z.msw & SIMUL_TRAP)
		ARITH_WARNING(ARITH_EXCEP_MINUS_U);
	else if (~x.msw & y.msw & z.msw & SIMUL_TRAP)
		ARITH_WARNING(ARITH_EXCEP_MINUS_O);
#endif
	return z;
}

/*
 * Since signed and unsigned widths are equal for the simulated type,
 * we can use the unsigned left shift function, which performs the
 * the checks on the type width.
 */
ARITH_DECL_BI_SI_S(lsh)
{
	arith_s z = arith_op_u(lsh)(x, y);

#ifdef ARITHMETIC_CHECKS
	if (x.msw & SIMUL_TRAP) ARITH_WARNING(ARITH_EXCEP_LSH_U);
	else {
		/*
		 * To check for possible overflow, we right shift the
		 * result. We need to make the shift count proper so that
		 * we do not emit a double-warning. Besides, the left shift
		 * could have been untruncated but yet affet the sign bit,
		 * so we must test this explicitly.
		 */
		arith_s w = arith_op_u(rsh)(z, (unsigned)y % SIMUL_NUMBITS);

		if ((z.msw & SIMUL_TRAP) || w.msw != x.msw || w.lsw != x.lsw)
			ARITH_WARNING(ARITH_EXCEP_LSH_O);
	}
#endif
	return z;
}

/*
 * We define that right shifting a negative value, besides being worth a
 * warning, duplicates the sign bit. This is the most useful and most
 * usually encountered behaviour, and the standard allows it.
 */
ARITH_DECL_BI_SI_S(rsh)
{
	int xn = (x.msw & SIMUL_TRAP) != 0;
	arith_s z = arith_op_u(rsh)(x, y);
	int gy = (unsigned)y % SIMUL_NUMBITS;

#ifdef ARITHMETIC_CHECKS
	if (xn) ARITH_WARNING(ARITH_EXCEP_RSH_N);
#endif
	if (xn && gy > 0) {
		if (gy <= SIMUL_MSW_WIDTH) {
			z.msw |= TMSW(~(SIMUL_MSW_MASK >> gy));
		} else {
			z.msw = SIMUL_MSW_MASK;
			z.lsw |= TLSW(~(SIMUL_LSW_MASK
				>> (gy - SIMUL_MSW_WIDTH)));
		}
	}
	return z;
}

ARITH_DECL_BI_SS_I(lt)
{
	int xn = (x.msw & SIMUL_TRAP) != 0;
	int yn = (y.msw & SIMUL_TRAP) != 0;

	if (xn == yn) {
		return x.msw < y.msw || (x.msw == y.msw && x.lsw < y.lsw);
	} else {
		return xn;
	}
}

ARITH_DECL_BI_SS_I(leq)
{
	int xn = (x.msw & SIMUL_TRAP) != 0;
	int yn = (y.msw & SIMUL_TRAP) != 0;

	if (xn == yn) {
		return x.msw < y.msw || (x.msw == y.msw && x.lsw <= y.lsw);
	} else {
		return xn;
	}
}

ARITH_DECL_BI_SS_I(gt)
{
	return arith_op_s(lt)(y, x);
}

ARITH_DECL_BI_SS_I(geq)
{
	return arith_op_s(leq)(y, x);
}

ARITH_DECL_BI_SS_I(same)
{
	return x.msw == y.msw && x.lsw == y.lsw;
}

ARITH_DECL_BI_SS_I(neq)
{
	return !arith_op_s(same)(x, y);
}

ARITH_DECL_BI_SS_S(and)
{
	return arith_op_u(and)(x, y);
}

ARITH_DECL_BI_SS_S(xor)
{
	return arith_op_u(xor)(x, y);
}

ARITH_DECL_BI_SS_S(or)
{
	return arith_op_u(or)(x, y);
}

/*
 * This function calculates the signed integer division, yielding
 * both quotient and remainder. The divider (y) MUST be non-zero.
 */
static void arith_op_s(sdiv)(arith_s x, arith_s y, arith_s *q, arith_s *r)
{
	arith_u a = x, b = y, c, d;
	int xn = 0, yn = 0;

	if (x.msw & SIMUL_TRAP) { a = arith_op_u(neg)(x); xn = 1; }
	if (y.msw & SIMUL_TRAP) { b = arith_op_u(neg)(y); yn = 1; }
	arith_op_u(udiv)(a, b, &c, &d);
	if (xn != yn) *q = arith_op_u(neg)(c); else *q = c;
	if (xn != yn) *r = arith_op_u(neg)(d); else *r = d;
}

/*
 * Overflow/underflow check is done the following way: obvious cases
 * are checked (both upper words non-null, both upper words null...)
 * and border-line occurrences are verified with an unsigned division
 * (which is quite computationaly expensive).
 */
ARITH_DECL_BI_SS_S(star)
{
#ifdef ARITHMETIC_CHECKS
	arith_s z = arith_op_u(star)(x, y);
	int warn = 0;

	if (x.msw > 0) {
		if (y.msw > 0
#if SIMUL_LSW_ODDLEN
			|| (y.lsw & SIMUL_TRAPL)
#endif
		) warn = 1;
	}
#if SIMUL_LSW_ODDLEN
	else if (y.msw > 0 && (x.lsw & SIMUL_TRAPL)) warn = 1;
#endif
	if (!warn && (x.msw > 0 || y.msw > 0
#if SIMUL_LSW_ODDLEN
		|| ((x.lsw | y.lsw) & SIMUL_TRAPL)
#endif
	)) {
		if (x.msw == SIMUL_MSW_MASK && x.lsw == SIMUL_LSW_MASK) {
			if (y.msw == SIMUL_TRAP && y.lsw == 0) warn = 1;
		} else if (!(x.msw == 0 && x.lsw == 0)
			&& !arith_op_s(same)(arith_op_s(slash)(z, x), y)) {
		} warn = 1;
	}
	if (warn) ARITH_WARNING(((x.msw ^ y.msw) & SIMUL_TRAP)
		? ARITH_EXCEP_STAR_U : ARITH_EXCEP_STAR_O);
	return z;
#else
	return arith_op_u(star)(x, y);
#endif
}

ARITH_DECL_BI_SS_S(slash)
{
	arith_s q, r;

	if (arith_op_s(same)(y, SIMUL_ZERO))
		ARITH_ERROR(ARITH_EXCEP_SLASH_D);
	else if (x.msw == SIMUL_TRAP && x.lsw == 0
		&& y.msw == SIMUL_MSW_MASK && y.lsw == SIMUL_LSW_MASK)
		ARITH_ERROR(ARITH_EXCEP_SLASH_O);
	arith_op_s(sdiv)(x, y, &q, &r);
	return q;
}

ARITH_DECL_BI_SS_S(pct)
{
	arith_s q, r;

	if (arith_op_s(same)(y, SIMUL_ZERO))
		ARITH_ERROR(ARITH_EXCEP_PCT_D);
	arith_op_s(sdiv)(x, y, &q, &r);
	return r;
}

ARITH_DECL_MONO_ST_US(octconst)
{
	arith_u z = { 0, 0 };

	for (; ARITH_OCTAL(*c); c ++) {
		unsigned w = ARITH_OVAL(*c);
		if (z.msw > (SIMUL_MSW_MASK / 8))
			ARITH_ERROR(ARITH_EXCEP_CONST_O);
		z = arith_op_u(lsh)(z, 3);
		z.lsw |= w;
	}
	*ru = z;
	if (z.msw & SIMUL_TRAP) {
		*sp = 0;
	} else {
		*rs = z;
		*sp = 1;
	}
	return c;
}

ARITH_DECL_MONO_ST_US(decconst)
{
#define ARITH_ALPHA_TRAP    (1U << (SIMUL_MSW_WIDTH - 1))
#define ARITH_ALPHA_MASK    (ARITH_ALPHA_TRAP | (ARITH_ALPHA_TRAP - 1))
#define ARITH_ALPHA     ((ARITH_ALPHA_MASK - 10 * (ARITH_ALPHA_TRAP / 5)) + 1)
#define ARITH_ALPHA_A   ((SIMUL_MSW_MASK - 10 * (SIMUL_TRAP / 5)) + 1)

	arith_u z = { 0, 0 };

	for (; ARITH_DECIM(*c); c ++) {
		unsigned w = ARITH_DVAL(*c);
		SIMUL_ARITH_SUBTYPE t;

		if (z.msw > (SIMUL_MSW_MASK / 10)
			|| (z.msw == (SIMUL_MSW_MASK / 10) &&
/* ARITH_ALPHA is between 1 and 9, inclusive. */
#if ARITH_ALPHA == 5
			z.lsw >= SIMUL_TRAPL
#else
			z.lsw > ((SIMUL_TRAPL / 5) * ARITH_ALPHA_A
			+ ((SIMUL_TRAPL % 5) * ARITH_ALPHA_A) / 5)
#endif
			)) ARITH_ERROR(ARITH_EXCEP_CONST_O);
		z = arith_op_u(plus)(arith_op_u(lsh)(z, 3),
			arith_op_u(lsh)(z, 1));
		t = TLSW(z.lsw + w);
		if (t < z.lsw) z.msw ++;
		z.lsw = t;
	}
	*ru = z;
	if (z.msw & SIMUL_TRAP) {
		*sp = 0;
	} else {
		*rs = z;
		*sp = 1;
	}
	return c;

#undef ARITH_ALPHA_A
#undef ARITH_ALPHA
#undef ARITH_ALPHA_TRAP
#undef ARITH_ALPHA_MASK
}

ARITH_DECL_MONO_ST_US(hexconst)
{
	arith_u z = { 0, 0 };

	for (; ARITH_HEXAD(*c); c ++) {
		unsigned w = ARITH_HVAL(*c);
		if (z.msw > (SIMUL_MSW_MASK / 16))
			ARITH_ERROR(ARITH_EXCEP_CONST_O);
		z = arith_op_u(lsh)(z, 4);
		z.lsw |= w;
	}
	*ru = z;
	if (z.msw & SIMUL_TRAP) {
		*sp = 0;
	} else {
		*rs = z;
		*sp = 1;
	}
	return c;
}

#endif

#undef ARITH_HVAL
#undef ARITH_HEXAD
#undef ARITH_DVAL
#undef ARITH_DECIM
#undef ARITH_OVAL
#undef ARITH_OCTAL
