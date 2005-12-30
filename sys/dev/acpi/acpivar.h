/*	$OpenBSD: acpivar.h,v 1.9 2005/12/30 05:59:40 tedu Exp $	*/
/*
 * Copyright (c) 2005 Thorsten Lockert <tholo@sigmasoft.com>
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

#ifndef _DEV_ACPI_ACPIVAR_H_
#define _DEV_ACPI_ACPIVAR_H_

#include <sys/timeout.h>

#ifdef ACPI_DEBUG
extern int acpi_debug;
#define dprintf(x...)	  do { if (acpi_debug) printf(x); } while(0)
#define dnprintf(n,x...)  do { if (acpi_debug > (n)) printf(x); } while(0)
#else
#define dprintf(x...)
#define dnprintf(n,x...)
#endif

struct klist;

struct acpi_attach_args {
	char		*aaa_name;
	bus_space_tag_t	 aaa_iot;
	bus_space_tag_t	 aaa_memt;
	void		*aaa_table;
	paddr_t		 aaa_pbase; /* Physical base address of ACPI tables */
	struct aml_node *aaa_node;
};

struct acpi_mem_map {
	vaddr_t		 baseva;
	u_int8_t	*va;
	size_t		 vsize;
	paddr_t		 pa;
};

struct acpi_q {
	SIMPLEQ_ENTRY(acpi_q)	 q_next;
	void			*q_table;
	u_int8_t		 q_data[0];
};

typedef SIMPLEQ_HEAD(, acpi_q) acpi_qhead_t;

#define ACPIREG_PM1A_STS    0x00
#define ACPIREG_PM1A_EN	    0x01
#define ACPIREG_PM1A_CNT    0x02
#define ACPIREG_PM1B_STS    0x03
#define ACPIREG_PM1B_EN	    0x04
#define ACPIREG_PM1B_CNT    0x05
#define ACPIREG_PM2_CNT	    0x06
#define ACPIREG_PM_TMR	    0x07
#define ACPIREG_GPE0_STS    0x08
#define ACPIREG_GPE0_EN	    0x09
#define ACPIREG_GPE1_STS    0x0A
#define ACPIREG_GPE1_EN	    0x0B
#define ACPIREG_SMICMD	    0x0C
#define ACPIREG_MAXREG	    0x0D

/* Special registers */
#define ACPIREG_PM1_STS	    0x0E
#define ACPIREG_PM1_EN	    0x0F
#define ACPIREG_PM1_CNT	    0x10

struct acpi_parsestate
{
	u_int8_t           *start;
	u_int8_t           *end;
	u_int8_t           *pos;
};

struct acpi_reg_map {
	bus_space_handle_t  ioh;
	int		    addr;
	int		    size;
	const char	   *name;
};

struct acpi_softc {
	struct device		 sc_dev;

	bus_space_tag_t		 sc_iot;
	bus_space_tag_t		 sc_memt;
#if 0
	bus_space_tag_t		 sc_pcit;
	bus_space_tag_t		 sc_smbust;
#endif

	/*
	 * First-level ACPI tables
	 */
	struct acpi_fadt	*sc_fadt;
	acpi_qhead_t		 sc_tables;

	/*
	 * Second-level information from FADT
	 */
	struct acpi_facs	*sc_facs;	/* Shared with firmware! */

	struct klist		*sc_note;
	struct acpi_reg_map	 sc_pmregs[ACPIREG_MAXREG];
	bus_space_handle_t	 sc_ioh_pm1a_evt;
  
	void			*sc_interrupt;
#ifdef __HAVE_GENERIC_SOFT_INTERRUPTS
	void			*sc_softih;
#else
	struct timeout		 sc_timeout;
#endif

	int			 sc_powerbtn;
	int			 sc_sleepbtn;

	struct acpi_parsestate   amlpc;
};

struct acpi_table {
	int	offset;
	size_t	size;
	void	*table;
};

#define	ACPI_IOC_GETFACS	_IOR('A', 0, struct acpi_facs)
#define	ACPI_IOC_GETTABLE	_IOWR('A', 1, struct acpi_table)
#define ACPI_IOC_SETSLEEPSTATE	_IOW('A', 2, int)

#define	ACPI_EV_PWRBTN		0x0001	/* Power button was pushed */
#define	ACPI_EV_SLPBTN		0x0002	/* Sleep button was pushed */

#define	ACPI_EVENT_MASK		0x0003

#define	ACPI_EVENT_COMPOSE(t,i)	(((i) & 0x7fff) << 16 | ((t) & ACPI_EVENT_MASK))
#define	ACPI_EVENT_TYPE(e)	((e) & ACPI_EVENT_MASK)
#define	ACPI_EVENT_INDEX(e)	((e) >> 16)

/*
 * Sleep states
 */
#define	ACPI_STATE_S5		5

#if defined(_KERNEL)
int	 acpi_map(paddr_t, size_t, struct acpi_mem_map *);
void	 acpi_unmap(struct acpi_mem_map *);
int	 acpi_probe(struct device *, struct cfdata *, struct acpi_attach_args *);
u_int	 acpi_checksum(const void *, size_t);
void	 acpi_attach_machdep(struct acpi_softc *);
int	 acpi_interrupt(void *);
void	 acpi_enter_sleep_state(struct acpi_softc *, int);
void	 acpi_powerdown(void);
#endif

#endif	/* !_DEV_ACPI_ACPIVAR_H_ */
