#ifndef LYCHARVALS_H
#define LYCHARVALS_H 1

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

/*
 * Use integer values for character constants rather than '\octal' form, since
 * not all compilers agree that those will fit in a character, even when cast.
 */
#ifndef   CH_ESC
#ifdef    EBCDIC
#define CH_DEL     	0x07
#define CH_ESC     	0x27
#define CH_ESC_PAR 	0x27
#define CH_HICTL   	0x3f
#define CH_NBSP    	0x41
#define CH_SHY     	0xca
#else  /* EBCDIC */
#define CH_ESC     	0033
#define CH_DEL     	0177
#define CH_ESC_PAR 	0233
#define CH_HICTL   	0237
#define CH_NBSP    	0240
#define CH_SHY     	0255
#endif /* EBCDIC */
#endif /* CH_ESC */

#endif /* LYCHARVALS_H */
