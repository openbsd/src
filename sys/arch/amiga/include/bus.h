/*	$OpenBSD: bus.h,v 1.2 1996/08/04 01:34:38 niklas Exp $	*/

/*
 * Copyright (c) 1996 Niklas Hallqvist.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Niklas Hallqvist.
 * 4. Neither the name of the University nor the names of its contributors
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
 */

#ifndef _MACHINE_BUS_H_
#define _MACHINE_BUS_H_

/* I/O access types.  */
typedef u_long bus_io_addr_t;
typedef u_long bus_io_size_t;
typedef u_long bus_io_handle_t;

/* Memory access types.  */
typedef u_long bus_mem_addr_t;
typedef u_long bus_mem_size_t;
typedef u_long bus_mem_handle_t;

/*
 * The big switch, that delegates each bus operation to the right
 * implementation.
 */
typedef struct amiga_bus_chipset *bus_chipset_tag_t;

struct amiga_bus_chipset {
	void	*bc_data;

	int	(*bc_io_map)(bus_chipset_tag_t, bus_io_addr_t, bus_io_size_t,
	    bus_io_handle_t *);
	int	(*bc_io_unmap)(bus_io_handle_t, bus_io_size_t);

	u_int8_t (*bc_io_read_1)(bus_io_handle_t, bus_io_size_t);
	u_int16_t (*bc_io_read_2)(bus_io_handle_t, bus_io_size_t);
	u_int32_t (*bc_io_read_4)(bus_io_handle_t, bus_io_size_t);
	u_int64_t (*bc_io_read_8)(bus_io_handle_t, bus_io_size_t);

	void	(*bc_io_read_multi_1)(bus_io_handle_t, bus_io_size_t,
		    u_int8_t *, bus_io_size_t);
	void	(*bc_io_read_multi_2)(bus_io_handle_t, bus_io_size_t,
		    u_int16_t *, bus_io_size_t);
	void	(*bc_io_read_multi_4)(bus_io_handle_t, bus_io_size_t,
		    u_int32_t *, bus_io_size_t);
	void	(*bc_io_read_multi_8)(bus_io_handle_t, bus_io_size_t,
		    u_int64_t *, bus_io_size_t);

	void	(*bc_io_write_1)(bus_io_handle_t, bus_io_size_t, u_int8_t);
	void	(*bc_io_write_2)(bus_io_handle_t, bus_io_size_t, u_int16_t);
	void	(*bc_io_write_4)(bus_io_handle_t, bus_io_size_t, u_int32_t);
	void	(*bc_io_write_8)(bus_io_handle_t, bus_io_size_t, u_int64_t);

	void	(*bc_io_write_multi_1)(bus_io_handle_t, bus_io_size_t,
		    const u_int8_t *, bus_io_size_t);
	void	(*bc_io_write_multi_2)(bus_io_handle_t, bus_io_size_t,
		    const u_int16_t *, bus_io_size_t);
	void	(*bc_io_write_multi_4)(bus_io_handle_t, bus_io_size_t,
		    const u_int32_t *, bus_io_size_t);
	void	(*bc_io_write_multi_8)(bus_io_handle_t, bus_io_size_t,
		    const u_int64_t *, bus_io_size_t);

	int	(*bc_mem_map)(bus_chipset_tag_t, bus_mem_addr_t,
		    bus_mem_size_t, int, bus_mem_handle_t *);
	int	(*bc_mem_unmap)(bus_mem_handle_t, bus_mem_size_t);

	u_int8_t (*bc_mem_read_1)(bus_mem_handle_t, bus_mem_size_t);
	u_int16_t (*bc_mem_read_2)(bus_mem_handle_t, bus_mem_size_t);
	u_int32_t (*bc_mem_read_4)(bus_mem_handle_t, bus_mem_size_t);
	u_int64_t (*bc_mem_read_8)(bus_mem_handle_t, bus_mem_size_t);

	void	(*bc_mem_write_1)(bus_mem_handle_t, bus_mem_size_t, u_int8_t);
	void	(*bc_mem_write_2)(bus_mem_handle_t, bus_mem_size_t, u_int16_t);
	void	(*bc_mem_write_4)(bus_mem_handle_t, bus_mem_size_t, u_int32_t);
	void	(*bc_mem_write_8)(bus_mem_handle_t, bus_mem_size_t, u_int64_t);

	/* These are extensions to the general NetBSD bus interface.  */
	void	(*bc_io_read_raw_multi_2)(bus_io_handle_t, bus_io_size_t,
		    u_int8_t *, bus_io_size_t);
	void	(*bc_io_read_raw_multi_4)(bus_io_handle_t, bus_io_size_t,
		    u_int8_t *, bus_io_size_t);
	void	(*bc_io_read_raw_multi_8)(bus_io_handle_t, bus_io_size_t,
		    u_int8_t *, bus_io_size_t);

	void	(*bc_io_write_raw_multi_2)(bus_io_handle_t, bus_io_size_t,
		    const u_int8_t *, bus_io_size_t);
	void	(*bc_io_write_raw_multi_4)(bus_io_handle_t, bus_io_size_t,
		    const u_int8_t *, bus_io_size_t);
	void	(*bc_io_write_raw_multi_8)(bus_io_handle_t, bus_io_size_t,
		    const u_int8_t *, bus_io_size_t);
};

#define bus_io_map(t, port, size, iohp) \
    (*(t)->bc_io_map)((t), (port), (size), (iohp))
#define bus_io_unmap(t, iohp, size) \
    (*(t)->bc_io_unmap)((iohp), (size))

#define	bus_io_read_1(t, h, o) \
    (*(t)->bc_io_read_1)((h), (o))
#define	bus_io_read_2(t, h, o) \
    (*(t)->bc_io_read_2)((h), (o))
#define	bus_io_read_4(t, h, o) \
    (*(t)->bc_io_read_4)((h), (o))
#define	bus_io_read_8(t, h, o) \
    (*(t)->bc_io_read_8)((h), (o))

#define	bus_io_read_multi_1(t, h, o, a, s) \
    (*(t)->bc_io_read_multi_1)((h), (o), (a), (s))
#define	bus_io_read_multi_2(t, h, o, a, s) \
    (*(t)->bc_io_read_multi_2)((h), (o), (a), (s))
#define	bus_io_read_multi_4(t, h, o, a, s) \
    (*(t)->bc_io_read_multi_4)((h), (o), (a), (s))
#define	bus_io_read_multi_8(t, h, o, a, s) \
    (*(t)->bc_io_read_multi_8)((h), (o), (a), (s))

#define	bus_io_write_1(t, h, o, v) \
    (*(t)->bc_io_write_1)((h), (o), (v))
#define	bus_io_write_2(t, h, o, v) \
    (*(t)->bc_io_write_2)((h), (o), (v))
#define	bus_io_write_4(t, h, o, v) \
    (*(t)->bc_io_write_4)((h), (o), (v))
#define	bus_io_write_8(t, h, o, v) \
    (*(t)->bc_io_write_8)((h), (o), (v))

#define	bus_io_write_multi_1(t, h, o, a, s) \
    (*(t)->bc_io_write_multi_1)((h), (o), (a), (s))
#define	bus_io_write_multi_2(t, h, o, a, s) \
    (*(t)->bc_io_write_multi_2)((h), (o), (a), (s))
#define	bus_io_write_multi_4(t, h, o, a, s) \
    (*(t)->bc_io_write_multi_4)((h), (o), (a), (s))
#define	bus_io_write_multi_8(t, h, o, a, s) \
    (*(t)->bc_io_write_multi_8)((h), (o), (a), (s))

#define bus_mem_map(t, port, size, cacheable, mhp) \
    (*(t)->bc_mem_map)((t), (port), (size), (cacheable), (mhp))
#define bus_mem_unmap(t, mhp, size) \
    (*(t)->bc_mem_unmap)((mhp), (size))

#define	bus_mem_read_1(t, h, o) \
    (*(t)->bc_mem_read_1)((h), (o))
#define	bus_mem_read_2(t, h, o) \
    (*(t)->bc_mem_read_2)((h), (o))
#define	bus_mem_read_4(t, h, o) \
    (*(t)->bc_mem_read_4)((h), (o))
#define	bus_mem_read_8(t, h, o) \
    (*(t)->bc_mem_read_8)((h), (o))

#define	bus_mem_write_1(t, h, o, v) \
    (*(t)->bc_mem_write_1)((h), (o), (v))
#define	bus_mem_write_2(t, h, o, v) \
    (*(t)->bc_mem_write_2)((h), (o), (v))
#define	bus_mem_write_4(t, h, o, v) \
    (*(t)->bc_mem_write_4)((h), (o), (v))
#define	bus_mem_write_8(t, h, o, v) \
    (*(t)->bc_mem_write_8)((h), (o), (v))

/* OpenBSD extensions */
#define	bus_io_read_raw_multi_2(t, h, o, a, s) \
    (*(t)->bc_io_read_raw_multi_2)((h), (o), (a), (s))
#define	bus_io_read_raw_multi_4(t, h, o, a, s) \
    (*(t)->bc_io_read_raw_multi_4)((h), (o), (a), (s))
#define	bus_io_read_raw_multi_8(t, h, o, a, s) \
    (*(t)->bc_io_read_raw_multi_8)((h), (o), (a), (s))

#define	bus_io_write_raw_multi_2(t, h, o, a, s) \
    (*(t)->bc_io_write_raw_multi_2)((h), (o), (a), (s))
#define	bus_io_write_raw_multi_4(t, h, o, a, s) \
    (*(t)->bc_io_write_raw_multi_4)((h), (o), (a), (s))
#define	bus_io_write_raw_multi_8(t, h, o, a, s) \
    (*(t)->bc_io_write_raw_multi_8)((h), (o), (a), (s))

#endif /* _MACHINE_BUS_H_ */
