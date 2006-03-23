/*	$OpenBSD: skipjack.h,v 1.4 2006/03/23 15:32:28 mickey Exp $	*/

/* 
 * Further optimized test implementation of SKIPJACK algorithm 
 * Mark Tillotson <markt@chaos.org.uk>, 25 June 98
 * Optimizations suit RISC (lots of registers) machine best.
 *
 * based on unoptimized implementation of
 * Panu Rissanen <bande@lut.fi> 960624
 *
 * SKIPJACK and KEA Algorithm Specifications 
 * Version 2.0 
 * 29 May 1998
*/

extern void skipjack_forwards(u_int8_t *plain, u_int8_t *cipher, u_int8_t **key);
extern void skipjack_backwards(u_int8_t *cipher, u_int8_t *plain, u_int8_t **key);
extern void subkey_table_gen(u_int8_t *key, u_int8_t **key_tables);
