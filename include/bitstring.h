/*	$NetBSD: bitstring.h,v 1.4 1994/10/26 00:55:45 cgd Exp $	*/

/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Paul Vixie.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	@(#)bitstring.h	5.2 (Berkeley) 4/4/90
 */

#ifndef _BITSTRING_H_
#define _BITSTRING_H_

/* modified for SV/AT and bitstring bugfix by M.R.Murphy, 11oct91
 * bitstr_size changed gratuitously, but shorter
 * bit_alloc   spelling error fixed
 * the following were efficient, but didn't work, they've been made to
 * work, but are no longer as efficient :-)
 * bit_nclear, bit_nset, bit_ffc, bit_ffs
 */
typedef	unsigned char bitstr_t;

/* internal macros */
				/* byte of the bitstring bit is in */
#define	_bit_byte(bit) \
	((bit) >> 3)

				/* mask for the bit within its byte */
#define	_bit_mask(bit) \
	(1 << ((bit)&0x7))

/* external macros */
				/* bytes in a bitstring of nbits bits */
#define	bitstr_size(nbits) \
	(((nbits) + 7) >> 3)

				/* allocate a bitstring */
#define	bit_alloc(nbits) \
	(bitstr_t *)calloc((size_t)bitstr_size(nbits), sizeof(bitstr_t))

				/* allocate a bitstring on the stack */
#define	bit_decl(name, nbits) \
	(name)[bitstr_size(nbits)]

				/* is bit N of bitstring name set? */
#define	bit_test(name, bit) \
	((name)[_bit_byte(bit)] & _bit_mask(bit))

				/* set bit N of bitstring name */
#define	bit_set(name, bit) \
	(name)[_bit_byte(bit)] |= _bit_mask(bit)

				/* clear bit N of bitstring name */
#define	bit_clear(name, bit) \
	(name)[_bit_byte(bit)] &= ~_bit_mask(bit)

				/* clear bits start ... stop in bitstring */
#define	bit_nclear(name, start, stop) do { \
	register bitstr_t *_name = name; \
	register int _start = start, _stop = stop; \
	while (_start <= _stop) { \
		bit_clear(_name, _start); \
		_start++; \
		} \
} while (0)

				/* set bits start ... stop in bitstring */
#define	bit_nset(name, start, stop) do { \
	register bitstr_t *_name = name; \
	register int _start = start, _stop = stop; \
	while (_start <= _stop) { \
		bit_set(_name, _start); \
		_start++; \
		} \
} while (0)

				/* find first bit clear in name */
#define	bit_ffc(name, nbits, value) do { \
	register bitstr_t *_name = name; \
	register int _bit, _nbits = nbits, _value = -1; \
	for (_bit = 0; _bit < _nbits; ++_bit) \
		if (!bit_test(_name, _bit)) { \
			_value = _bit; \
			break; \
		} \
	*(value) = _value; \
} while (0)

				/* find first bit set in name */
#define	bit_ffs(name, nbits, value) do { \
	register bitstr_t *_name = name; \
	register int _bit, _nbits = nbits, _value = -1; \
	for (_bit = 0; _bit < _nbits; ++_bit) \
		if (bit_test(_name, _bit)) { \
			_value = _bit; \
			break; \
		} \
	*(value) = _value; \
} while (0)

#endif /* !_BITSTRING_H_ */
