/*	$OpenBSD: tcfsdefines.h,v 1.4 2000/06/19 20:35:47 fgsch Exp $	*/

/*
 *	Transparent Cryptographic File System (TCFS) for NetBSD 
 *	Author and mantainer: 	Luigi Catuogno [luicat@tcfs.unisa.it]
 *	
 *	references:		http://tcfs.dia.unisa.it
 *				tcfs-bsd@tcfs.unisa.it
 */

/*
 *	Base utility set v0.1
 */

#ifndef _TCFSDEFINES_H_
#define _TCFSDEFINES_H_

#define UUKEYSIZE	((KEYSIZE / 3 + (KEYSIZE % 3 ? 1 : 0)) * 4)
#define GKEYSIZE	(KEYSIZE + KEYSIZE/8)
#define UUGKEYSIZE	((GKEYSIZE / 3 + (GKEYSIZE % 3 ? 1 : 0)) * 4)
#define TRUE		1
#define FALSE		0
#define ONE		1 /* decrement key counter by 1 */
#define ALL		0 /* decrement key counter to 0 */
#define SET		1 /* set permanent flag */
#define UNSET		0 /* unset permanent flag */
#define NONE		2 /* no one of the previous */

#define USERKEY		0
#define GROUPKEY	1

typedef struct {
         u_int32_t flag;
} tcfs_flags;

#endif /* _TCFSDEFINES_H_ */
