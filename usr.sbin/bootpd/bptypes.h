/* bptypes.h */

#ifndef	BPTYPES_H
#define	BPTYPES_H

/*
 * 32 bit integers are different types on various architectures
 * XXX THE CORRECT WAY TO DO THIS IS:
 * XXX	(1) convert to _t form for all uses,
 * XXX	(2) define the _t's here (or somewhere)
 * XXX		if !defined(__BIT_TYPES_DEFINED__)
 */

typedef int32_t int32;
typedef u_int32_t u_int32;

/*
 * Nice typedefs. . .
 */

typedef int boolean;
typedef unsigned char byte;


#endif	/* BPTYPES_H */
