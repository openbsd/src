/*	$OpenBSD: pucvar.h,v 1.13 2011/11/15 22:27:53 deraadt Exp $	*/
/*	$NetBSD: pucvar.h,v 1.2 1999/02/06 06:29:54 cgd Exp $	*/

/*
 * Copyright (c) 1998, 1999 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Exported (or conveniently located) PCI "universal" communications card
 * software structures.
 *
 * Author: Christopher G. Demetriou, May 14, 1998.
 */

#define	PUC_MAX_PORTS		16

struct puc_device_description {
	u_int16_t	rval[4];
	u_int16_t	rmask[4];
	struct {
		u_char	type;
		u_char	bar;
		u_short	offset;
	}			ports[PUC_MAX_PORTS];
};

/*
 * For serial ports, the type field also encodes a multiplier
 * of the speed.
 */
#define PUC_LPT			(0x00 | 0x40)
#define PUC_COM_MUL(mul)	(0x80 | 0x40 | (mul))
#define PUC_COM_POW2(pow2)	(0x80 |        (pow2))

#define PUC_IS_LPT(type)	(((type) & 0xc0) == 0x40)
#define PUC_IS_COM(type)	(((type) & 0x80) == 0x80)

#define PUC_IS_COM_MUL(type)	((type) & 0x40)
#define PUC_COM_GET_MUL(type)	((type) & 0x3f)
#define PUC_COM_GET_POW2(type)	((type) & 0x3f)

#define PUC_PORT_BAR_INDEX(bar)	(((bar) - PCI_MAPREG_START) / 4)

struct puc_attach_args {
	int			port;
	int			type;
	void			*puc;

	bus_addr_t		a;
	bus_space_tag_t		t;
	bus_space_handle_t	h;

	const char *(*intr_string)(struct puc_attach_args *);
	void *(*intr_establish)(struct puc_attach_args *, int, int (*)(void *),
	    void *, char *);
};

extern const struct puc_device_description puc_devs[];
extern int puc_ndevs;

#define	PUC_NBARS	6
struct puc_softc {
	struct device		sc_dev;

	/* static configuration data */
	const struct puc_device_description *sc_desc;

	/* card-global dynamic data */
	struct {
		int		mapped;
		bus_addr_t	a;
		bus_size_t	s;
		bus_space_tag_t	t;
		bus_space_handle_t h;
	} sc_bar_mappings[PUC_NBARS];

	/* per-port dynamic data */
	struct {
		struct device   *dev;
		/* filled in by port attachments */
		void	*intrhand;
	} sc_ports[PUC_MAX_PORTS];
};

const struct puc_device_description *
    puc_find_description(u_int16_t, u_int16_t, u_int16_t, u_int16_t);
void	puc_print_ports(const struct puc_device_description *);
void	puc_common_attach(struct puc_softc *, struct puc_attach_args *);
int	puc_print(void *, const char *);
int	puc_submatch(struct device *, void *, void *);
