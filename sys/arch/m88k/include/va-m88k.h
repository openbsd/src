/*	$OpenBSD: va-m88k.h,v 1.11 2010/12/31 20:37:36 miod Exp $	*/

/* Define __gnuc_va_list.  */

#ifndef __GNUC_VA_LIST
#define __GNUC_VA_LIST

#include <sys/cdefs.h>

typedef struct __va_list_tag {
	unsigned int  __va_arg;		/* argument number */
	unsigned int *__va_stk;		/* start of args passed on stack */
	unsigned int *__va_reg;		/* start of args passed in regs */
} __gnuc_va_list[1];

#endif /* not __GNUC_VA_LIST */

/* If this is for internal libc use, don't define anything but
   __gnuc_va_list.  */
#if defined (_STDARG_H) || defined (_VARARGS_H)

#define __va_start_common(AP,FAKE) \
__extension__ ({							\
   (AP) = (struct __va_list_tag *)__builtin_alloca(sizeof(__gnuc_va_list)); \
  __builtin_memcpy ((AP), __builtin_saveregs (), sizeof(__gnuc_va_list)); \
  })

#ifdef _STDARG_H /* stdarg.h support */

/* Calling __builtin_next_arg gives the proper error message if LASTARG is
   not indeed the last argument.  */
#ifdef lint
#define va_start(AP,LASGARG) ((AP) = (AP))
#else
#define va_start(AP,LASTARG) \
  (__builtin_next_arg (LASTARG), __va_start_common (AP, 0))
#endif /* lint */

#else /* varargs.h support */

#ifdef lint
#define va_start(AP) ((AP) = (AP))
#else
#define va_start(AP) __va_start_common (AP, 1)
#endif /* lint */

#define va_alist __va_1st_arg
#define va_dcl register int va_alist; ...

#endif /* _STDARG_H */

#define __va_reg_p(TYPE)						\
__extension__({								\
    __builtin_classify_type(*(TYPE *)0) < 12 ? /* record, union */	\
	sizeof(TYPE) <= 8 :						\
	sizeof(TYPE) == 4 && __alignof__(*(TYPE *)0) == 4;		\
})

#define	__va_size(TYPE) ((sizeof(TYPE) + 3) >> 2)

/* We cast to void * and then to TYPE * because this avoids
   a warning about increasing the alignment requirement.  */
#define va_arg(AP,TYPE)							\
__extension__(*({							\
    register TYPE *__ptr;						\
									\
    if ((AP)->__va_arg <= 8 && __va_reg_p(TYPE)) {			\
	/* might be in registers */					\
	if (((AP)->__va_arg & 1) != 0 && __alignof__(*(TYPE *)0) > 4)	\
	    (AP)->__va_arg++;						\
	(AP)->__va_arg += __va_size(TYPE);				\
    }									\
									\
    if ((AP)->__va_arg <= 8 && __va_reg_p(TYPE)) {			\
	__ptr = (TYPE *) (void *) ((AP)->__va_reg +			\
	    (AP)->__va_arg - __va_size(TYPE));				\
    } else {								\
	if (((unsigned int)((AP)->__va_stk) & 4) != 0 &&		\
	    __alignof__(*(TYPE *)0) > 4) {				\
	    (AP)->__va_stk++;						\
	} \
	__ptr = (TYPE *) (AP)->__va_stk;				\
	(AP)->__va_stk += __va_size(TYPE);				\
    }									\
    __ptr;								\
}))

#define va_end(AP)	((void)0)

#ifdef lint
#define	__va_copy(dest, src)	((dest) = (src))
#else
/* Copy __gnuc_va_list into another variable of this type.  */
#define __va_copy(dest, src) \
__extension__ ({ \
	(dest) =  \
	   (struct __va_list_tag *)__builtin_alloca(sizeof(__gnuc_va_list)); \
	*(dest) = *(src);\
  })
#endif /* lint */

#endif /* defined (_STDARG_H) || defined (_VARARGS_H) */
