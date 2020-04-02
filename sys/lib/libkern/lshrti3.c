/* ===-- lshrti3.c - Implement __lshrti3 -----------------------------------===
 *
 *                     The LLVM Compiler Infrastructure
 *
 * This file is dual licensed under the MIT and the University of Illinois Open
 * Source Licenses. See LICENSE.TXT for details.
 *
 * ===----------------------------------------------------------------------===
 *
 * This file implements __lshrti3 for the compiler_rt library.
 *
 * ===----------------------------------------------------------------------===
 */

/* Returns: logical a >> b */

/* Precondition:  0 <= b < bits_in_tword */

#include <sys/limits.h>
#include <sys/endian.h>

typedef int      si_int;
typedef unsigned int su_int;
typedef          long long di_int;
typedef unsigned long long du_int;
typedef int      ti_int __attribute__ ((mode (TI)));
typedef int      tu_int __attribute__ ((mode (TI)));

#if BYTE_ORDER == LITTLE_ENDIAN
#define _YUGA_LITTLE_ENDIAN 0
#else
#define _YUGA_LITTLE_ENDIAN 1
#endif

typedef union
{
	tu_int all;
	struct
	{
#if _YUGA_LITTLE_ENDIAN
		du_int low;
		du_int high;
#else
		du_int high;
		du_int low;
#endif /* _YUGA_LITTLE_ENDIAN */
	}s;
} utwords;


ti_int
__lshrti3(ti_int a, si_int b)
{
    const int bits_in_dword = (int)(sizeof(di_int) * CHAR_BIT);
    utwords input;
    utwords result;
    input.all = a;
    if (b & bits_in_dword)  /* bits_in_dword <= b < bits_in_tword */
    {
        result.s.high = 0;
        result.s.low = input.s.high >> (b - bits_in_dword);
    }
    else  /* 0 <= b < bits_in_dword */
    {
        if (b == 0)
            return a;
        result.s.high  = input.s.high >> b;
        result.s.low = (input.s.high << (bits_in_dword - b)) | (input.s.low >> b);
    }
    return result.all;
}
