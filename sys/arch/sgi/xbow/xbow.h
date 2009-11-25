/*	$OpenBSD: xbow.h,v 1.10 2009/11/25 11:23:30 miod Exp $	*/

/*
 * Copyright (c) 2008 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef	_XBOW_H_
#define	_XBOW_H_

/*
 * Devices connected to the XBow are called ``widgets'' and are
 * identified by a common widget memory area at the beginning of their
 * memory space.
 *
 * Each widget has its own memory space.  The lowest 16MB are always
 * accessible as a so-called ``short window''.  Other `views' of the
 * widget are possible, depending on the system (the whole widget
 * address space is always visible on Octane, while Origin family
 * systems can only map a few ``large windows'', which are a scarce
 * resource).
 *
 * Apart from the crossbow itself being widget #0, the widgets are divided
 * in two groups: widgets #8 to #b are the ``upper'' widgets, while widgets
 * #c to #f are the ``lower'' widgets.
 *
 * Widgets are uniquely identified with their widget number on the XBow
 * bus. However, the way they are mapped and accessed will depend on the
 * processor (well, the processor board node) requesting access. Hence the
 * two parameters needed to map a widget.
 */

extern	paddr_t (*xbow_widget_base)(int16_t, u_int);
extern	paddr_t	(*xbow_widget_map)(int16_t, u_int, bus_addr_t *, bus_size_t *);

extern	int	(*xbow_widget_id)(int16_t, u_int, uint32_t *);
extern	int	xbow_intr_widget;
extern	paddr_t	xbow_intr_widget_register;

extern	int	(*xbow_intr_widget_intr_register)(int, int, int *);
extern	int	(*xbow_intr_widget_intr_establish)(int (*)(void *), void *,
		    int, int, const char *, struct intrhand *);
extern	void	(*xbow_intr_widget_intr_disestablish)(int);

extern	void	(*xbow_intr_widget_intr_set)(int);
extern	void	(*xbow_intr_widget_intr_clear)(int);

/*
 * Valid widget values
 */

#define	WIDGET_MIN			8
#define	WIDGET_MAX			15

struct xbow_attach_args {
	int16_t		xaa_nasid;
	int		xaa_widget;

	uint32_t	xaa_vendor;
	uint32_t	xaa_product;
	uint32_t	xaa_revision;

	bus_space_tag_t xaa_iot;
	/*
	 * WARNING! xaa_iot points to memory allocated on the stack,
	 * drivers need to make a copy of it.
	 */
};

void	xbow_build_bus_space(struct mips_bus_space *, int, int);
int	xbow_intr_register(int, int, int *);
int	xbow_intr_establish(int (*)(void *), void *, int, int, const char *,
	    struct intrhand *);
void	xbow_intr_disestablish(int);
void	xbow_intr_clear(int);
void	xbow_intr_set(int);

paddr_t	xbow_widget_map_space(struct device *, u_int,
	    bus_addr_t *, bus_size_t *);

int	xbow_space_map(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);
uint8_t xbow_read_1(bus_space_tag_t, bus_space_handle_t, bus_size_t);
uint16_t xbow_read_2(bus_space_tag_t, bus_space_handle_t, bus_size_t);
void	xbow_read_raw_2(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    uint8_t *, bus_size_t);
void	xbow_write_1(bus_space_tag_t, bus_space_handle_t, bus_size_t, uint8_t);
void	xbow_write_2(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    uint16_t);
void	xbow_write_raw_2(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const uint8_t *, bus_size_t);

/*
 * Widget register access routines hiding addressing games depending upon
 * the access width.
 */
static __inline__ uint32_t
widget_read_4(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t a)
{
	return bus_space_read_4(t, h, a | 4);
}
static __inline__ uint64_t
widget_read_8(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t a)
{
	return bus_space_read_8(t, h, a);
}
static __inline__ void
widget_write_4(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t a,
    uint32_t v)
{
	bus_space_write_4(t, h, a | 4, v);
}
static __inline__ void
widget_write_8(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t a,
    uint64_t v)
{
	bus_space_write_8(t, h, a, v);
}

#endif	/* _XBOW_H_ */
