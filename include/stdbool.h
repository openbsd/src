/* $OpenBSD: stdbool.h,v 1.2 1999/09/24 23:09:09 espie Exp $ */

/*
 * Written by Marc Espie, September 25, 1999
 * Public domain.
 */

#ifndef	_STDBOOL_H_
#define	_STDBOOL_H_	

/* `_Bool' type must promote to `int' or `unsigned int'. */
typedef enum {
	false = 0,
	true = 1
} _Bool;

/* And those constants must also be available as macros. */
#define	false	false
#define	true	true

/* User visible type `bool' is provided as a macro which may be redefined */
#define bool _Bool

/* Inform that everything is fine */
#define __bool_true_false_are_defined 1

#endif /* _STDBOOL_H_ */
