/*	$OpenBSD: stdarg.h,v 1.3 2005/12/14 21:46:31 millert Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)stdarg.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _MIPS_STDARG_H_
#define	_MIPS_STDARG_H_

#include <sys/cdefs.h>
#include <machine/ansi.h>

typedef _BSD_VA_LIST_	va_list;

#ifndef __GNUC_VA_LIST
#define __GNUC_VA_LIST
#if defined (__mips_eabi) && ! defined (__mips_soft_float) && ! defined (__mips_single_float)

typedef struct {
  /* Pointer to FP regs.  */
  char *__fp_regs;
  /* Number of FP regs remaining.  */
  int __fp_left;
  /* Pointer to GP regs followed by stack parameters.  */
  char *__gp_regs;
} __gnuc_va_list;

#else /* ! (defined (__mips_eabi) && ! defined (__mips_soft_float) && ! defined (__mips_single_float)) */

typedef char * __gnuc_va_list;

#endif /* ! (defined (__mips_eabi) && ! defined (__mips_soft_float) && ! defined (__mips_single_float)) */
#endif /* not __GNUC_VA_LIST */

/* If this is for internal libc use, don't define anything but __gnuc_va_list */

#ifndef _VA_MIPS_H_ENUM
#define _VA_MIPS_H_ENUM
enum {
  __no_type_class = -1,
  __void_type_class,
  __integer_type_class,
  __char_type_class,
  __enumeral_type_class,
  __boolean_type_class,
  __pointer_type_class,
  __reference_type_class,
  __offset_type_class,
  __real_type_class,
  __complex_type_class,
  __function_type_class,
  __method_type_class,
  __record_type_class,
  __union_type_class,
  __array_type_class,
  __string_type_class,
  __set_type_class,
  __file_type_class,
  __lang_type_class
};
#endif

#define __va_ellipsis ...

#ifdef __mips64
#define __va_rounded_size(__TYPE)  \
  (((sizeof (__TYPE) + 8 - 1) / 8) * 8)
#else
#define __va_rounded_size(__TYPE)  \
  (((sizeof (__TYPE) + sizeof (int) - 1) / sizeof (int)) * sizeof (int))
#endif

#ifdef __mips64
#define __va_reg_size 8
#else
#define __va_reg_size 4
#endif

#if defined (__mips_eabi)
#if ! defined (__mips_soft_float) && ! defined (__mips_single_float)
#ifdef __mips64
#define va_start(__AP, __LASTARG)					\
  (__AP.__gp_regs = ((char *) __builtin_next_arg (__LASTARG)		\
		     - (__builtin_args_info (2) < 8			\
			? (8 - __builtin_args_info (2)) * __va_reg_size	\
			: 0)),						\
   __AP.__fp_left = 8 - __builtin_args_info (3),			\
   __AP.__fp_regs = __AP.__gp_regs - __AP.__fp_left * __va_reg_size)
#else /* ! defined (__mips64) */
#define va_start(__AP, __LASTARG)					\
  (__AP.__gp_regs = ((char *) __builtin_next_arg (__LASTARG)		\
		     - (__builtin_args_info (2) < 8			\
			? (8 - __builtin_args_info (2)) * __va_reg_size	\
			: 0)),						\
   __AP.__fp_left = (8 - __builtin_args_info (3)) / 2,			\
   __AP.__fp_regs = __AP.__gp_regs - __AP.__fp_left * 8,		\
   __AP.__fp_regs = (char *) ((int) __AP.__fp_regs & -8))
#endif /* ! defined (__mips64) */
#else /* ! (! defined (__mips_soft_float) && ! defined (__mips_single_float) ) */
#define va_start(__AP, __LASTARG)					\
  (__AP = ((__gnuc_va_list) __builtin_next_arg (__LASTARG)		\
	   - (__builtin_args_info (2) >= 8 ? 0				\
	      : (8 - __builtin_args_info (2)) * __va_reg_size)))
#endif /* ! (! defined (__mips_soft_float) && ! defined (__mips_single_float) ) */
#else /* ! defined (__mips_eabi) */
#define va_start(__AP, __LASTARG) \
  (__AP = (__gnuc_va_list) __builtin_next_arg (__LASTARG))
#endif /* ! (defined (__mips_eabi) && ! defined (__mips_soft_float) && ! defined (__mips_single_float)) */

#ifndef va_end
void va_end (__gnuc_va_list);		/* Defined in libgcc.a */
#endif
#define va_end(__AP)	((void)0)

#if defined (__mips_eabi)

#if ! defined (__mips_soft_float) && ! defined (__mips_single_float)
#ifdef __mips64
#define __va_next_addr(__AP, __type)					\
  ((__builtin_classify_type (*(__type *) 0) == __real_type_class	\
    && __AP.__fp_left > 0)						\
   ? (--__AP.__fp_left, (__AP.__fp_regs += 8) - 8)			\
   : (__AP.__gp_regs += __va_reg_size) - __va_reg_size)
#else
#define __va_next_addr(__AP, __type)					\
  ((__builtin_classify_type (*(__type *) 0) == __real_type_class	\
    && __AP.__fp_left > 0)						\
   ? (--__AP.__fp_left, (__AP.__fp_regs += 8) - 8)			\
   : (((__builtin_classify_type (* (__type *) 0) < __record_type_class	\
	&& __alignof__ (__type) > 4)					\
       ? __AP.__gp_regs = (char *) (((int) __AP.__gp_regs + 8 - 1) & -8) \
       : (char *) 0),							\
      (__builtin_classify_type (* (__type *) 0) >= __record_type_class	\
       ? (__AP.__gp_regs += __va_reg_size) - __va_reg_size		\
       : ((__AP.__gp_regs += __va_rounded_size (__type))		\
	  - __va_rounded_size (__type)))))
#endif
#else /* ! (! defined (__mips_soft_float) && ! defined (__mips_single_float)) */
#ifdef __mips64
#define __va_next_addr(__AP, __type)					\
  ((__AP += __va_reg_size) - __va_reg_size)
#else
#define __va_next_addr(__AP, __type)					\
  (((__builtin_classify_type (* (__type *) 0) < __record_type_class	\
     && __alignof__ (__type) > 4)					\
    ? __AP = (char *) (((__PTRDIFF_TYPE__) __AP + 8 - 1) & -8)		\
    : (char *) 0),							\
   (__builtin_classify_type (* (__type *) 0) >= __record_type_class	\
    ? (__AP += __va_reg_size) - __va_reg_size				\
    : ((__AP += __va_rounded_size (__type))				\
       - __va_rounded_size (__type))))
#endif
#endif /* ! (! defined (__mips_soft_float) && ! defined (__mips_single_float)) */

#ifdef __MIPSEB__
#define va_arg(__AP, __type)						\
  ((__va_rounded_size (__type) <= __va_reg_size)			\
   ? *(__type *) (void *) (__va_next_addr (__AP, __type)		\
			   + __va_reg_size				\
			   - sizeof (__type))				\
   : (__builtin_classify_type (*(__type *) 0) >= __record_type_class	\
      ? **(__type **) (void *) (__va_next_addr (__AP, __type)		\
				+ __va_reg_size				\
				- sizeof (char *))			\
      : *(__type *) (void *) __va_next_addr (__AP, __type)))
#else
#define va_arg(__AP, __type)						\
  ((__va_rounded_size (__type) <= __va_reg_size)			\
   ? *(__type *) (void *) __va_next_addr (__AP, __type)		\
   : (__builtin_classify_type (* (__type *) 0) >= __record_type_class	\
      ? **(__type **) (void *) __va_next_addr (__AP, __type)		\
      : *(__type *) (void *) __va_next_addr (__AP, __type)))
#endif

#else /* ! defined (__mips_eabi) */

/* We cast to void * and then to TYPE * because this avoids
   a warning about increasing the alignment requirement.  */
/* The __mips64 cases are reversed from the 32 bit cases, because the standard
   32 bit calling convention left-aligns all parameters smaller than a word,
   whereas the __mips64 calling convention does not (and hence they are
   right aligned).  */
#ifdef __mips64
#ifdef __MIPSEB__
#define va_arg(__AP, __type)                                    \
  ((__type *) (void *) (__AP = (char *)                         \
                       ((((__PTRDIFF_TYPE__)__AP + 8 - 1) & -8) \
			   + __va_rounded_size (__type))))[-1]
#else
#define va_arg(__AP, __type)                                    \
  ((__AP = (char *) ((((__PTRDIFF_TYPE__)__AP + 8 - 1) & -8)	\
		     + __va_rounded_size (__type))),		\
   *(__type *) (void *) (__AP - __va_rounded_size (__type)))
#endif

#else /* not __mips64 */

#ifdef __MIPSEB__
/* For big-endian machines.  */
#define va_arg(__AP, __type)					\
  ((__AP = (char *) ((__alignof__ (__type) > 4			\
		      ? ((__PTRDIFF_TYPE__)__AP + 8 - 1) & -8	\
		      : ((__PTRDIFF_TYPE__)__AP + 4 - 1) & -4)	\
		     + __va_rounded_size (__type))),		\
   *(__type *) (void *) (__AP - __va_rounded_size (__type)))
#else
/* For little-endian machines.  */
#define va_arg(__AP, __type)						    \
  ((__type *) (void *) (__AP = (char *) ((__alignof__(__type) > 4	    \
				? ((__PTRDIFF_TYPE__)__AP + 8 - 1) & -8	    \
				: ((__PTRDIFF_TYPE__)__AP + 4 - 1) & -4)    \
					 + __va_rounded_size(__type))))[-1]
#endif
#endif
#endif /* ! defined (__mips_eabi)  */

/* Copy __gnuc_va_list into another variable of this type.  */
#define __va_copy(dest, src) (dest) = (src)
#if __ISO_C_VISIBLE >= 1999
#define va_copy __va_copy
#endif

#endif /* !_MIPS_STDARG_H_ */
