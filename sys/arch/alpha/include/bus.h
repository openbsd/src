/*	$NetBSD: bus.h,v 1.2.4.2 1996/06/13 17:44:45 cgd Exp $	*/

/*
 * Copyright (c) 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#ifndef _ALPHA_BUS_H_
#define _ALPHA_BUS_H_

/*
 * I/O addresses (in bus space)
 */
typedef u_long bus_io_addr_t;
typedef u_long bus_io_size_t;

/*
 * Memory addresses (in bus space)
 */
typedef u_long bus_mem_addr_t;
typedef u_long bus_mem_size_t;

/*
 * Access methods for bus resources, I/O space, and memory space.
 */
typedef struct alpha_bus_chipset *bus_chipset_tag_t;
typedef u_long bus_io_handle_t;
typedef u_long bus_mem_handle_t;

struct alpha_bus_chipset {
	/* I/O-space cookie */
	void		*bc_i_v;

	/* I/O-space control functions */
	int		(*bc_i_map) __P((void *v, bus_io_addr_t port,
			    bus_io_size_t size, bus_io_handle_t *iohp));
	void		(*bc_i_unmap) __P((void *v, bus_io_handle_t ioh,
			    bus_io_size_t size));
	int		(*bc_i_subregion) __P((void *v, bus_io_handle_t ioh,
			    bus_io_size_t offset, bus_io_size_t size,
			    bus_io_handle_t *nioh));

	/* I/O-space read functions */
	u_int8_t	(*bc_ir1) __P((void *v, bus_io_handle_t ioh,
			    bus_io_size_t off));
	u_int16_t	(*bc_ir2) __P((void *v, bus_io_handle_t ioh,
			    bus_io_size_t off));
	u_int32_t	(*bc_ir4) __P((void *v, bus_io_handle_t ioh,
			    bus_io_size_t off));
	u_int64_t	(*bc_ir8) __P((void *v, bus_io_handle_t ioh,
			    bus_io_size_t off));

	/* I/O-space read-multiple functions */
	void		(*bc_irm1) __P((void *v, bus_io_handle_t ioh,
			    bus_io_size_t off, u_int8_t *addr,
			    bus_io_size_t count));
	void		(*bc_irm2) __P((void *v, bus_io_handle_t ioh,
			    bus_io_size_t off, u_int16_t *addr,
			    bus_io_size_t count));
	void		(*bc_irm4) __P((void *v, bus_io_handle_t ioh,
			    bus_io_size_t off, u_int32_t *addr,
			    bus_io_size_t count));
	void		(*bc_irm8) __P((void *v, bus_io_handle_t ioh,
			    bus_io_size_t off, u_int64_t *addr,
			    bus_io_size_t count));

	/* I/O-space write functions */
	void		(*bc_iw1) __P((void *v, bus_io_handle_t ioh,
			    bus_io_size_t off, u_int8_t val));
	void		(*bc_iw2) __P((void *v, bus_io_handle_t ioh,
			    bus_io_size_t off, u_int16_t val));
	void		(*bc_iw4) __P((void *v, bus_io_handle_t ioh,
			    bus_io_size_t off, u_int32_t val));
	void		(*bc_iw8) __P((void *v, bus_io_handle_t ioh,
			    bus_io_size_t off, u_int64_t val));

	/* I/O-space write-multiple functions */
	void		(*bc_iwm1) __P((void *v, bus_io_handle_t ioh,
			    bus_io_size_t off, const u_int8_t *addr,
			    bus_io_size_t count));
	void		(*bc_iwm2) __P((void *v, bus_io_handle_t ioh,
			    bus_io_size_t off, const u_int16_t *addr,
			    bus_io_size_t count));
	void		(*bc_iwm4) __P((void *v, bus_io_handle_t ioh,
			    bus_io_size_t off, const u_int32_t *addr,
			    bus_io_size_t count));
	void		(*bc_iwm8) __P((void *v, bus_io_handle_t ioh,
			    bus_io_size_t off, const u_int64_t *addr,
			    bus_io_size_t count));

	/* Mem-space cookie */
	void		*bc_m_v;

	/* Mem-space control functions */
	int		(*bc_m_map) __P((void *v, bus_mem_addr_t buspa,
			    bus_mem_size_t size, int cacheable,
			    bus_mem_handle_t *mhp));
	void		(*bc_m_unmap) __P((void *v, bus_mem_handle_t mh,
			    bus_mem_size_t size));
	int		(*bc_m_subregion) __P((void *v, bus_mem_handle_t memh,
			    bus_mem_size_t offset, bus_mem_size_t size,
			    bus_mem_handle_t *nmemh));

	/* Mem-space read functions */
	u_int8_t	(*bc_mr1) __P((void *v, bus_mem_handle_t memh,
			    bus_mem_size_t off));
	u_int16_t	(*bc_mr2) __P((void *v, bus_mem_handle_t memh,
			    bus_mem_size_t off));
	u_int32_t	(*bc_mr4) __P((void *v, bus_mem_handle_t memh,
			    bus_mem_size_t off));
	u_int64_t	(*bc_mr8) __P((void *v, bus_mem_handle_t memh,
			    bus_mem_size_t off));

	/* Mem-space write functions */
	void		(*bc_mw1) __P((void *v, bus_mem_handle_t memh,
			    bus_mem_size_t off, u_int8_t val));
	void		(*bc_mw2) __P((void *v, bus_mem_handle_t memh,
			    bus_mem_size_t off, u_int16_t val));
	void		(*bc_mw4) __P((void *v, bus_mem_handle_t memh,
			    bus_mem_size_t off, u_int32_t val));
	void		(*bc_mw8) __P((void *v, bus_mem_handle_t memh,
			    bus_mem_size_t off, u_int64_t val));

	/* XXX THIS DOES NOT YET BELONG HERE */
	vm_offset_t	(*bc_XXX_dmamap) __P((void *addr));
};

#define __bc_CONCAT(A,B)	__CONCAT(A,B)
#define __bc_ABC(A,B,C)		__bc_CONCAT(A,__bc_CONCAT(B,C))
#define __bc_ABCD(A,B,C,D)	__bc_CONCAT(__bc_ABC(A,B,C),D)

#define __bc_rd(t, h, o, sz, sp)					\
    (*(t)->__bc_ABCD(bc_,sp,r,sz))((t)->__bc_ABC(bc_,sp,_v), h, o)

#define __bc_wr(t, h, o, v, sz, sp)					\
    (*(t)->__bc_ABCD(bc_,sp,w,sz))((t)->__bc_ABC(bc_,sp,_v), h, o, v)

#define bus_io_map(t, port, size, iohp)					\
    (*(t)->bc_i_map)((t)->bc_i_v, (port), (size), (iohp))
#define bus_io_unmap(t, ioh, size)					\
    (*(t)->bc_i_unmap)((t)->bc_i_v, (ioh), (size))
#define bus_io_subregion(t, ioh, offset, size, nioh)			\
    (*(t)->bc_i_unmap)((t)->bc_i_v, (ioh), (offset), (size), (nioh))

#define	__bc_io_multi(t, h, o, a, s, dir, sz)				\
    (*(t)->__bc_ABCD(bc_i,dir,m,sz))((t)->bc_i_v, h, o, a, s)

#define	bus_io_read_1(t, h, o)		__bc_rd((t),(h),(o),1,i)
#define	bus_io_read_2(t, h, o)		__bc_rd((t),(h),(o),2,i)
#define	bus_io_read_4(t, h, o)		__bc_rd((t),(h),(o),4,i)
#define	bus_io_read_8(t, h, o)		__bc_rd((t),(h),(o),8,i)

#define	bus_io_read_multi_1(t, h, o, a, s)				\
    __bc_io_multi((t),(h),(o),(a),(s),r,1)
#define	bus_io_read_multi_2(t, h, o, a, s)				\
    __bc_io_multi((t),(h),(o),(a),(s),r,2)
#define	bus_io_read_multi_4(t, h, o, a, s)				\
    __bc_io_multi((t),(h),(o),(a),(s),r,4)
#define	bus_io_read_multi_8(t, h, o, a, s)				\
    __bc_io_multi((t),(h),(o),(a),(s),r,8)

#define	bus_io_write_1(t, h, o, v)	__bc_wr((t),(h),(o),(v),1,i)
#define	bus_io_write_2(t, h, o, v)	__bc_wr((t),(h),(o),(v),2,i)
#define	bus_io_write_4(t, h, o, v)	__bc_wr((t),(h),(o),(v),4,i)
#define	bus_io_write_8(t, h, o, v)	__bc_wr((t),(h),(o),(v),8,i)

#define	bus_io_write_multi_1(t, h, o, a, s)				\
    __bc_io_multi((t),(h),(o),(a),(s),w,1)
#define	bus_io_write_multi_2(t, h, o, a, s)				\
    __bc_io_multi((t),(h),(o),(a),(s),w,2)
#define	bus_io_write_multi_4(t, h, o, a, s)				\
    __bc_io_multi((t),(h),(o),(a),(s),w,4)
#define	bus_io_write_multi_8(t, h, o, a, s)				\
    __bc_io_multi((t),(h),(o),(a),(s),w,8)

#define bus_mem_map(t, bpa, size, cacheable, mhp)			\
    (*(t)->bc_m_map)((t)->bc_m_v, (bpa), (size), (cacheable), (mhp))
#define bus_mem_unmap(t, memh, size)					\
    (*(t)->bc_m_unmap)((t)->bc_m_v, (memh), (size))
#define bus_mem_subregion(t, memh, offset, size, nmemh)			\
    (*(t)->bc_m_unmap)((t)->bc_i_v, (memh), (offset), (size), (nmemh))

#define	bus_mem_read_1(t, h, o)		__bc_rd((t),(h),(o),1,m)
#define	bus_mem_read_2(t, h, o)		__bc_rd((t),(h),(o),2,m)
#define	bus_mem_read_4(t, h, o)		__bc_rd((t),(h),(o),4,m)
#define	bus_mem_read_8(t, h, o)		__bc_rd((t),(h),(o),8,m)

#define	bus_mem_write_1(t, h, o, v)	__bc_wr((t),(h),(o),(v),1,m)
#define	bus_mem_write_2(t, h, o, v)	__bc_wr((t),(h),(o),(v),2,m)
#define	bus_mem_write_4(t, h, o, v)	__bc_wr((t),(h),(o),(v),4,m)
#define	bus_mem_write_8(t, h, o, v)	__bc_wr((t),(h),(o),(v),8,m)

/* XXX THIS DOES NOT BELONG HERE YET. */
#define	__alpha_bus_XXX_dmamap(t, va) (*(t)->bc_XXX_dmamap)((va))

#endif /* _ALPHA_BUS_H_ */
