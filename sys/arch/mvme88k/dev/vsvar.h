/*	$OpenBSD: vsvar.h,v 1.17 2004/09/06 06:25:28 miod Exp $	*/
/*
 * Copyright (c) 2004, Miodrag Vallat.
 * Copyright (c) 1999 Steve Murphree, Jr.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Van Jacobson of Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
#ifndef	_VSVAR_H_
#define	_VSVAR_H_

/*
 * The largest single request will be MAXPHYS bytes which will require
 * at most MAXPHYS/NBPG+1 chain elements to describe, i.e. if none of
 * the buffer pages are physically contiguous (MAXPHYS/NBPG) and the
 * buffer is not page aligned (+1).
 */
#define	DMAMAXIO	(MAXPHYS/NBPG+1)
#define	OFF(x)	(u_int16_t)(x)

/*
 * scatter/gather structures
 */

typedef struct {
	union {
		unsigned short bytes :16;
#define	MAX_SG_BLOCK_SIZE	(1<<16)
		struct {
			unsigned short :8;
			unsigned short gather:8;
		} scatter;
	} count;
	u_int16_t addrhi, addrlo;	/* split due to alignment */
	unsigned short link:1;
	unsigned short :3;
	unsigned short transfer_type:2;
#define	SHORT_TRANSFER			0x1
#define	LONG_TRANSFER			0x2
#define	SCATTER_GATTER_LIST_IN_SHORT_IO	0x3
	unsigned short memory_type:2;
#define	NORMAL_TYPE	0x0
#define	BLOCK_MODE	0x1
	unsigned short address_modifier:8;
} sg_list_element_t;

typedef sg_list_element_t * scatter_gather_list_t;

#define	MAX_SG_ELEMENTS	64

struct m328_sg {
	struct m328_sg *up;
	int elements;
	int level;
	struct m328_sg *down[MAX_SG_ELEMENTS];
	sg_list_element_t list[MAX_SG_ELEMENTS];
};

typedef struct m328_sg *M328_SG;

typedef struct {
	struct scsi_xfer *xs;
	M328_SG top_sg_list;
} M328_CMD;

struct vs_softc {
	struct device		sc_dev;
	paddr_t			sc_paddr;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	struct intrhand		sc_ih_e, sc_ih_n;
	char			sc_intrname_e[16 + 4];
	int			sc_ipl;
	int			sc_evec, sc_nvec;
	int			sc_id[2];
	struct scsi_link	sc_link[2];
};

/* Access macros */

#define	vs_read(w,o) \
	bus_space_read_##w (sc->sc_iot, sc->sc_ioh, (o))
#define	vs_write(w,o,v) \
	bus_space_write_##w (sc->sc_iot, sc->sc_ioh, (o), (v))
#define	vs_bzero(o,s) \
	bus_space_set_region_2(sc->sc_iot, sc->sc_ioh, (o), 0, (s) / 2)

#define	cib_read(w,o)		vs_read(w, sh_CIB + (o))
#define	cib_write(w,o,v)	vs_write(w, sh_CIB + (o), (v))
#define	crb_read(w,o)		vs_read(w, sh_CRB + (o))
#define	crb_write(w,o,v)	vs_write(w, sh_CRB + (o), (v))
#define	csb_read(w,o)		vs_read(w, sh_CSS + (o))
#define	mce_read(w,o)		vs_read(w, sh_MCE + (o))
#define	mce_write(w,o,v)	vs_write(w, sh_MCE + (o), (v))
#define	mce_iopb_read(w,o)	vs_read(w, sh_MCE_IOPB + (o))
#define	mce_iopb_write(w,o,v)	vs_write(w, sh_MCE_IOPB + (o), (v))
#define	mcsb_read(w,o)		vs_read(w, sh_MCSB + (o))
#define	mcsb_write(w,o,v)	vs_write(w, sh_MCSB + (o), (v))

#define	CRSW		crb_read(2, CRB_CRSW)
#define	CRB_CLR_DONE	crb_write(2, CRB_CRSW, 0)
#define	CRB_CLR_ER	crb_write(2, CRB_CRSW, CRSW & ~M_CRSW_ER)

#define	THAW_REG	mcsb_read(2, MCSB_THAW)
#define	THAW(x) \
	do { \
		mcsb_write(1, MCSB_THAW, (x)); \
		mcsb_write(1, MCSB_THAW + 1, M_THAW_TWQE); \
	} while (0)

#endif /* _M328VAR_H */
