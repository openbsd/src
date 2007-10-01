/*	$OpenBSD: if_sandrv.c,v 1.12 2007/10/01 15:34:48 krw Exp $	*/

/*-
 * Copyright (c) 2001-2004 Sangoma Technologies (SAN)
 * All rights reserved.  www.sangoma.com
 *
 * This code is written by Alex Feldman <al.feldman@sangoma.com> for SAN.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Sangoma Technologies nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SANGOMA TECHNOLOGIES AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#define __SDLA_HW_LEVEL
#define __SDLADRV__

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/malloc.h>
#include <sys/kernel.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/if_san_front_end.h>
#include <dev/pci/if_sandrv.h>

#define	EXEC_DELAY	20	/* shared memory access delay, mks */
#define EXEC_TIMEOUT	(hz*2)
#define MAX_NLOOPS	(EXEC_DELAY*2000)
			/* timeout used if jiffies are stopped
			** EXEC_DELAY=20
			** EXEC_TIMEOUT=EXEC_DELAY*2000 = 40000
			** 40000 ~= 80 jiffies = EXEC_TIMEOUT */

#define	EXEC_HZ_DIVISOR	8/10
			/* We don't want to wait a full second on sdla_exec
			** timeout, thus use HZ * EXEC_HZ_DIVISOR to get
			** the number of jiffies we would like to wait */

#define IS_SUPPORTED_ADAPTER(hw)	((hw)->type == SDLA_AFT)

#define SDLA_CTYPE_NAME(type)			\
	((type) == SDLA_AFT) ? "AFT" : "Unknown"

#define IS_AFT(hw)	(hw->type == SDLA_AFT)

/* Definitions for identifying and finding S514 PCI adapters */
#define V3_VENDOR_ID		0x11B0		/* V3 vendor ID number */
#define V3_DEVICE_ID		0x0002		/* V3 device ID number */
#define SANGOMA_SUBSYS_VENDOR	0x4753		/* ID for Sangoma */

/* Definition for identifying and finding XILINX PCI adapters */
#define SANGOMA_PCI_VENDOR	0x1923		/* Old value -> 0x11B0 */
#define SANGOMA_PCI_VENDOR_OLD	0x10EE		/* Old value -> 0x11B0 */
#define SANGOMA_PCI_DEVICE	0x0300		/* Old value -> 0x0200 */

#define A101_1TE1_SUBSYS_VENDOR	0xA010	/* A101 with T1/E1 1 line  */
#define A101_2TE1_SUBSYS_VENDOR	0xA011	/* A101 with T1/E1 2 lines */
#define A105_T3_SUBSYS_VENDOR	0xA020	/* A102 with T3 */

/* Read PCI SUBVENDOR ID */
#define PCI_SUBVENDOR_MASK	0xFFFF
#define PCI_SUBVENDOR(pa)	(pci_conf_read(pa->pa_pc, pa->pa_tag,	\
				    PCI_SUBSYS_ID_REG) & PCI_SUBVENDOR_MASK)
#define PCI_DEVICE_MASK		0xFFFF0000
#define PCI_DEVICE(id)		((id & PCI_DEVICE_MASK ) >> 16)

/* Status values */
#define SDLA_MEM_RESERVED	0x0001
#define SDLA_MEM_MAPPED		0x0002
#define SDLA_IO_MAPPED		0x0004
#define SDLA_PCI_ENABLE		0x0008

struct san_softc {
	struct device		dev;
	struct pci_attach_args	pa;
};

typedef struct sdla_hw_probe {
	int				used;
	unsigned char			hw_info[100];
	LIST_ENTRY(sdla_hw_probe)	next;
} sdla_hw_probe_t;

/*
 * This structure keeps common parameters per physical card.
 */
typedef struct sdlahw_card {
	int			used;
	unsigned int		type;		/* S50x/S514/ADSL/XILINX */
	unsigned int		atype;		/* SubVendor ID */
	unsigned char		core_id;	/* SubSystem ID [0..7] */
	unsigned char		core_rev;	/* SubSystem ID [8..15] */
	unsigned char		pci_extra_ver;
	unsigned int		slot_no;
	unsigned int		bus_no;
	bus_space_tag_t		memt;
	struct pci_attach_args	pa;	/* PCI config header info */
	pci_intr_handle_t	ih;
	LIST_ENTRY(sdlahw_card)	next;
} sdlahw_card_t;

/*
 * Adapter hardware configuration. Pointer to this structure is passed to all
 * APIs.
 */
typedef struct sdlahw {
	int			 used;
	unsigned		 magic;
	char			 devname[20];
	u_int16_t		 status;
	int			 irq;		/* interrupt request level */
	unsigned int		 cpu_no;	/* PCI CPU Number */
	char			 auto_pci_cfg;	/* Auto PCI configuration */
	bus_addr_t		 mem_base_addr;
	bus_space_handle_t	 dpmbase;	/* dual-port memory base */
	unsigned		 dpmsize;	/* dual-port memory size */
	unsigned long		 memory;	/* memory size */

	unsigned		 reserved[5];
	unsigned char		 hw_info[100];

	u_int16_t		 configured;
	void			*arg;		/* card structure */
	sdla_hw_probe_t		*hwprobe;
	sdlahw_card_t		*hwcard;
	LIST_ENTRY(sdlahw)	 next;
} sdlahw_t;

/* Entry Point for Low-Level function */
int sdladrv_init(void);
int sdladrv_exit(void);

static int sdla_pci_probe(int, struct pci_attach_args *);

/* PCI bus interface function */
static int sdla_pci_write_config_word(void *, int, u_int16_t);
static int sdla_pci_write_config_dword(void *, int, u_int32_t);
static int sdla_pci_read_config_byte(void *, int, u_int8_t *);
static int sdla_pci_read_config_word(void *, int, u_int16_t *);
static int sdla_pci_read_config_dword(void *, int, u_int32_t *);

static int sdla_detect	(sdlahw_t *);
static int sdla_detect_aft(sdlahw_t *);
static int sdla_exec(sdlahw_t *, unsigned long);
static void sdla_peek_by_4(sdlahw_t *, unsigned long, void *, unsigned int);
static void sdla_poke_by_4(sdlahw_t *, unsigned long, void *, unsigned int);

static sdlahw_card_t* sdla_card_register(u_int16_t, int, int);
#if 0
static int sdla_card_unregister (unsigned char, int, int, int);
#endif
static sdlahw_card_t* sdla_card_search(u_int16_t, int, int);

static sdlahw_t* sdla_hw_register(sdlahw_card_t *, int, int, void *);
#if 0
static int sdla_hw_unregister(sdlahw_card_t*, int);
#endif
static sdlahw_t* sdla_hw_search(u_int16_t, int, int, int);

static sdlahw_t* sdla_aft_hw_select (sdlahw_card_t *, int, int,
    struct pci_attach_args *);
static void sdla_save_hw_probe (sdlahw_t*, int);

/* SDLA PCI device relative entry point */
int	san_match(struct device *, void *, void *);
void	san_attach(struct device *, struct device *, void *);


struct cfdriver san_cd = {
	NULL, "san", DV_IFNET
};

struct cfattach san_ca = {
	sizeof(struct san_softc), san_match, san_attach
};

extern int ticks;

/* SDLA ISA/PCI varibles */
static int       Sangoma_cards_no = 0;
static int       Sangoma_devices_no = 0;
static int       Sangoma_PCI_cards_no = 0;

/* private data */
char		*san_drvname = "san";

/* Array of already initialized PCI slots */
static int pci_slot_ar[MAX_S514_CARDS];

LIST_HEAD(, sdlahw_card) sdlahw_card_head =
	LIST_HEAD_INITIALIZER(sdlahw_card_head);
LIST_HEAD(, sdlahw) sdlahw_head =
	LIST_HEAD_INITIALIZER(sdlahw_head);
LIST_HEAD(, sdla_hw_probe) sdlahw_probe_head =
	LIST_HEAD_INITIALIZER(sdlahw_probe_head);
static sdla_hw_type_cnt_t sdla_adapter_cnt;



/*
 * PCI Device Driver Entry Points
 */
int
san_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args*	pa = aux;
	u_int16_t		vendor_id = PCI_VENDOR(pa->pa_id);
	u_int16_t		device_id = PCI_DEVICE(pa->pa_id);

	if ((vendor_id == SANGOMA_PCI_VENDOR ||
	    vendor_id == SANGOMA_PCI_VENDOR_OLD) &&
	    device_id == SANGOMA_PCI_DEVICE) {
		return (1);
	}
	return (0);
}

#define PCI_CBIO	0x10
void
san_attach(struct device *parent, struct device *self, void *aux)
{
	struct pci_attach_args*		pa = aux;
	u_int16_t			vendor_id = PCI_VENDOR(pa->pa_id);
	u_int16_t			subvendor_id = PCI_SUBVENDOR(pa);
	int				atype = 0x00;

	atype = PCI_PRODUCT(pci_conf_read(pa->pa_pc, pa->pa_tag,
	    PCI_SUBSYS_ID_REG));
	switch (vendor_id) {
	case SANGOMA_PCI_VENDOR_OLD:
	case SANGOMA_PCI_VENDOR:
		switch (subvendor_id) {
		case A101_1TE1_SUBSYS_VENDOR:
			atype	= A101_ADPTR_1TE1;
			break;
		case A101_2TE1_SUBSYS_VENDOR:
			atype	= A101_ADPTR_2TE1;
			break;
		default:
			return;
		}
		break;
	default:
		return;
	}

	if (sdla_pci_probe(atype, pa)) {
		printf(": PCI probe FAILED!\n");
		return;
	}

#ifdef DEBUG
	switch (PCI_VENDOR(pa->pa_id)) {
	case V3_VENDOR_ID:
		switch (atype) {
		case S5141_ADPTR_1_CPU_SERIAL:
			log(LOG_INFO, "%s: Sangoma S5141/FT1 (Single CPU) "
			    "adapter\n", self->dv_xname);
			break;
		case S5142_ADPTR_2_CPU_SERIAL:
			log(LOG_INFO, "%s: Sangoma S5142 (Dual CPU) adapter\n",
			    self->dv_xname);
			break;
		case S5143_ADPTR_1_CPU_FT1:
			log(LOG_INFO, "%s: Sangoma S5143 (Single CPU) "
			    "FT1 adapter\n", self->dv_xname);
			break;
		case S5144_ADPTR_1_CPU_T1E1:
		case S5148_ADPTR_1_CPU_T1E1:
			log(LOG_INFO, "%s: Sangoma S5144 (Single CPU) "
			    "T1/E1 adapter\n", self->dv_xname);
			break;
		case S5145_ADPTR_1_CPU_56K:
			log(LOG_INFO, "%s: Sangoma S5145 (Single CPU) "
			    "56K adapter\n", self->dv_xname);
			break;
		case S5147_ADPTR_2_CPU_T1E1:
			log(LOG_INFO, "%s: Sangoma S5147 (Dual CPU) "
			    "T1/E1 adapter\n", self->dv_xname);
			break;
		}
		break;

	case SANGOMA_PCI_VENDOR_OLD:
		switch (atype) {
		case A101_ADPTR_1TE1:
			log(LOG_INFO, "%s: Sangoma AFT (1 channel) "
			    "T1/E1 adapter\n", self->dv_xname);
			break;
		case A101_ADPTR_2TE1:
			log(LOG_INFO, "%s: Sangoma AFT (2 channels) "
			    "T1/E1 adapter\n", self->dv_xname);
			break;
		}
		break;
	}
#endif
	return;
}

/*
 * Module init point.
 */
int
sdladrv_init(void)
{
	int volatile i = 0;

	/* Initialize the PCI Card array, which
	 * will store flags, used to mark
	 * card initialization state */
	for (i=0; i<MAX_S514_CARDS; i++)
		pci_slot_ar[i] = 0xFF;

	bzero(&sdla_adapter_cnt, sizeof(sdla_hw_type_cnt_t));

	return (0);
}

/*
 * Module deinit point.
 * o release all remaining system resources
 */
int
sdladrv_exit(void)
{
#if 0
	sdla_hw_probe_t	*elm_hw_probe;
	sdlahw_t	*elm_hw;
	sdlahw_card_t	*elm_hw_card;


	elm_hw = LIST_FIRST(&sdlahw_head);
	while (elm_hw) {
		sdlahw_t	*tmp = elm_hw;
		elm_hw = LIST_NEXT(elm_hw, next);
		if (sdla_hw_unregister(tmp->hwcard, tmp->cpu_no) == EBUSY)
			return EBUSY;
	}
	LIST_INIT(&sdlahw_head);

	elm_hw_card = LIST_FIRST(&sdlahw_card_head);
	while (elm_hw_card) {
		sdlahw_card_t	*tmp = elm_hw_card;
		elm_hw_card = LIST_NEXT(elm_hw_card, next);
		if (sdla_card_unregister(tmp->hw_type,
					 tmp->slot_no,
					 tmp->bus_no,
					 tmp->ioport) == EBUSY)
			return EBUSY;
	}
	LIST_INIT(&sdlahw_card_head);

	elm_hw_probe = LIST_FIRST(&sdlahw_probe_head);
	while (elm_hw_probe) {
		sdla_hw_probe_t *tmp = elm_hw_probe;
		elm_hw_probe = LIST_NEXT(elm_hw_probe, next);
		if (tmp->used){
			log(LOG_INFO, "HW probe info is in used (%s)\n",
					elm_hw_probe->hw_info);
			return EBUSY;
		}
		LIST_REMOVE(tmp, next);
		free(tmp, M_DEVBUF);
	}
#endif
	return (0);
}

static void
sdla_save_hw_probe(sdlahw_t *hw, int port)
{
	sdla_hw_probe_t *tmp_hw_probe;

	tmp_hw_probe = malloc(sizeof(*tmp_hw_probe), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (tmp_hw_probe == NULL)
		return;

	snprintf(tmp_hw_probe->hw_info, sizeof(tmp_hw_probe->hw_info),
		"%s : SLOT=%d : BUS=%d : IRQ=%d : CPU=%c : PORT=%s",
		SDLA_ADPTR_DECODE(hw->hwcard->atype), hw->hwcard->slot_no,
		hw->hwcard->bus_no, hw->irq, SDLA_GET_CPU(hw->cpu_no), "PRI");

	hw->hwprobe = tmp_hw_probe;
	tmp_hw_probe->used++;
	LIST_INSERT_HEAD(&sdlahw_probe_head, tmp_hw_probe, next);
}

static sdlahw_t*
sdla_aft_hw_select(sdlahw_card_t *hwcard, int cpu_no,
    int irq, struct pci_attach_args *pa)
{
	sdlahw_t*	hw = NULL;
	int		number_of_cards = 0;

	hwcard->type = SDLA_AFT;
	switch (hwcard->atype) {
	case A101_ADPTR_1TE1:
		hw = sdla_hw_register(hwcard, cpu_no, irq, pa);
		sdla_save_hw_probe(hw, 0);
		number_of_cards += 1;
#ifdef DEBUG
		log(LOG_INFO, "%s: %s T1/E1 card found (%s rev.%d), "
		    "cpu(s) 1, bus #%d, slot #%d, irq #%d\n", san_drvname,
		     SDLA_ADPTR_DECODE(hwcard->atype),
		     AFT_CORE_ID_DECODE(hwcard->core_id), hwcard->core_rev,
		     hwcard->bus_no, hwcard->slot_no, irq);
#endif /* DEBUG */
		break;
	case A101_ADPTR_2TE1:
		hw = sdla_hw_register(hwcard, cpu_no, irq, pa);
		sdla_save_hw_probe(hw, 0);
		number_of_cards += 1;
#ifdef DEBUG
		log(LOG_INFO, "%s: %s T1/E1 card found (%s rev.%d), "
		    "cpu(s) 2, bus #%d, slot #%d, irq #%d\n", san_drvname,
		    SDLA_ADPTR_DECODE(hwcard->atype),
		    AFT_CORE_ID_DECODE(hwcard->core_id), hwcard->core_rev,
		    hwcard->bus_no, hwcard->slot_no, irq);
#endif /* DEBUG */
		break;
	case A105_ADPTR_1_CHN_T3E3:

		hw = sdla_hw_register(hwcard, cpu_no, irq, pa);
		sdla_save_hw_probe(hw, 0);
		number_of_cards += 1;
#ifdef DEBUG
		log(LOG_INFO, "%s: %s T3/E3 card found, cpu(s) 1,"
		     "bus #%d, slot #%d, irq #%d\n", san_drvname,
		     SDLA_ADPTR_DECODE(hwcard->atype),
		     hwcard->bus_no, hwcard->slot_no, irq);
#endif /* DEBUG */
		break;
	default:
		log(LOG_INFO, "%s: Unknown adapter %04X "
		    "(bus #%d, slot #%d, irq #%d)!\n", san_drvname,
		    hwcard->atype, hwcard->bus_no, hwcard->slot_no, irq);
		break;
	}

	return (hw);
}


static int
sdla_pci_probe(int atype, struct pci_attach_args *pa)
{
	sdlahw_card_t*	hwcard;
	sdlahw_t*	hw;
	/*sdladev_t*	dev = NULL;*/
	int dual_cpu = 0;
	int bus, slot, cpu = SDLA_CPU_A;
	u_int16_t vendor_id, subvendor_id, device_id;
	u_int8_t irq;
	pci_intr_handle_t	ih;
	const char*			intrstr = NULL;

	bus = pa->pa_bus;
	slot = pa->pa_device;
	vendor_id = PCI_VENDOR(pa->pa_id);
	subvendor_id = PCI_SUBVENDOR(pa);
	device_id = PCI_DEVICE(pa->pa_id);
	irq = (u_int8_t)pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_INTLINE);

	/* Map and establish the interrupt */
	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		return (EINVAL);
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);
	if (intrstr != NULL)
		printf(" %s\n", intrstr);

	Sangoma_cards_no ++;
reg_new_card:
	Sangoma_PCI_cards_no ++;
	hwcard = sdla_card_register(atype, slot, bus);
	if (hwcard == NULL)
		return (EINVAL);

	hwcard->memt	= pa->pa_memt;
	hwcard->ih	= ih;
	hwcard->pa	= *pa;
	/* Increment number of available Sangoma devices */
	Sangoma_devices_no ++;
	switch (atype) {
	case A101_ADPTR_1TE1:
	case A101_ADPTR_2TE1:
		hw = sdla_aft_hw_select(hwcard, cpu, irq, pa);
		sdla_adapter_cnt.AFT_adapters++;
		if (atype == A101_ADPTR_2TE1)
			dual_cpu = 1;
		break;

	}

	if (hw == NULL)
	    return (EINVAL);
	if (san_dev_attach(hw, hw->devname, sizeof(hw->devname)))
		return (EINVAL);

	hw->used++;

	if (dual_cpu && cpu == SDLA_CPU_A) {
		cpu = SDLA_CPU_B;
		goto reg_new_card;
	}

	return (0);
}

int
sdla_intr_establish(void *phw, int (*intr_func)(void*), void* intr_arg)
{
	sdlahw_t	*hw = (sdlahw_t*)phw;
	sdlahw_card_t	*hwcard;

	WAN_ASSERT(hw == NULL);
	hwcard = hw->hwcard;
	if (pci_intr_establish(hwcard->pa.pa_pc, hwcard->ih, IPL_NET,
	    intr_func, intr_arg, "san") == NULL)
		return (EINVAL);

	return 0;
}

int
sdla_intr_disestablish(void *phw)
{
	sdlahw_t	*hw = (sdlahw_t*)phw;

	log(LOG_INFO, "%s: Disestablish interrupt is not defined!\n",
	    hw->devname);
	return (EINVAL);
}

int
sdla_get_hw_devices(void)
{
	return (Sangoma_devices_no);
}

void*
sdla_get_hw_adptr_cnt(void)
{
	return (&sdla_adapter_cnt);
}

static sdlahw_card_t*
sdla_card_register(u_int16_t atype, int slot_no, int bus_no)
{
	sdlahw_card_t	*new_hwcard, *last_hwcard;

	new_hwcard = sdla_card_search(atype, slot_no, bus_no);
	if (new_hwcard)
		return (new_hwcard);

	new_hwcard = malloc(sizeof(*new_hwcard), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!new_hwcard)
		return (NULL);

	new_hwcard->atype	= atype;
	new_hwcard->slot_no	= slot_no;
	new_hwcard->bus_no	= bus_no;

	if (LIST_EMPTY(&sdlahw_card_head)) {
		/* Initialize SAN HW parameters */
		sdladrv_init();
	}
	LIST_FOREACH(last_hwcard, &sdlahw_card_head, next) {
		if (!LIST_NEXT(last_hwcard, next))
			break;
	}

	if (last_hwcard)
		LIST_INSERT_AFTER(last_hwcard, new_hwcard, next);
	else
		LIST_INSERT_HEAD(&sdlahw_card_head, new_hwcard, next);

	return (new_hwcard);
}

#if 0
static int
sdla_card_unregister(u_int16_t atype, int slot_no, int bus_no, int ioport)
{
	sdlahw_card_t*	tmp_card;

	LIST_FOREACH(tmp_card, &sdlahw_card_head, next){
		if (tmp_card->atype != atype){
			continue;
		}
		if (tmp_card->slot_no == slot_no &&
					tmp_card->bus_no == bus_no){
			break;
		}
	}
	if (tmp_card == NULL){
		log(LOG_INFO,
		"Error: Card didn't find %04X card (slot=%d, bus=%d)\n"
				atype, slot_no, bus_no);
		return (EFAULT)
	}
	if (tmp_card->used){
		log(LOG_INFO,
		"Error: Card is still in used (slot=%d,bus=%d,used=%d)\n",
				slot_no, bus_no, tmp_card->used);
		return (EBUSY);
	}
	LIST_REMOVE(tmp_card, next);
	free(tmp_card, M_DEVBUF);
	return 0;
}
#endif

static sdlahw_card_t*
sdla_card_search(u_int16_t atype, int slot_no, int bus_no)
{
	sdlahw_card_t*	tmp_card;

	LIST_FOREACH(tmp_card, &sdlahw_card_head, next) {
		if (tmp_card->atype != atype)
			continue;

		if (tmp_card->slot_no == slot_no &&
		    tmp_card->bus_no == bus_no)
			return (tmp_card);
	}
	return (NULL);
}

static sdlahw_t*
sdla_hw_register(sdlahw_card_t *card, int cpu_no, int irq, void *dev)
{
	sdlahw_t	*new_hw, *last_hw;

	new_hw = sdla_hw_search(card->atype, card->slot_no,
	    card->bus_no, cpu_no);
	if (new_hw)
		return (new_hw);

	new_hw = malloc(sizeof(*new_hw), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!new_hw)
		return (NULL);

	new_hw->cpu_no	= cpu_no;
	new_hw->irq	= irq;
	new_hw->hwcard	= card;
#if 0
	new_hw->dev	= dev;
#endif
	new_hw->magic	= SDLAHW_MAGIC;
	card->used++;

	LIST_FOREACH(last_hw, &sdlahw_head, next) {
		if (!LIST_NEXT(last_hw, next))
			break;
	}
	if (last_hw)
		LIST_INSERT_AFTER(last_hw, new_hw, next);
	else
		LIST_INSERT_HEAD(&sdlahw_head, new_hw, next);

	return (new_hw);
}

#if 0
static int
sdla_hw_unregister(sdlahw_card_t* hwcard, int cpu_no)
{
	sdlahw_t*	tmp_hw;
	int		i;

	LIST_FOREACH(tmp_hw, &sdlahw_head, next) {
		if (tmp_hw->hwcard != hwcard)
			continue;

		if (tmp_hw->cpu_no == cpu_no)
			break;
	}

	if (tmp_hw == NULL) {
		log(LOG_INFO,
		"Error: Failed to find device (slot=%d,bus=%d,cpu=%c)\n",
		hwcard->slot_no, hwcard->bus_no, SDLA_GET_CPU(cpu_no));
		return (EFAULT);
	}
	if (tmp_hw->used) {
		log(LOG_INFO,
		"Error: Device is still in used (slot=%d,bus=%d,cpu=%c,%d)\n",
				hwcard->slot_no,
				hwcard->bus_no,
				SDLA_GET_CPU(cpu_no),
				hwcard->used);
		return (EBUSY);
	}

	tmp_hw->hwprobe = NULL;
	tmp_hw->hwcard = NULL;
	hwcard->used--;			/* Decrement card usage */
	LIST_REMOVE(tmp_hw, next);
	free(tmp_hw, M_DEVBUF);

	return (0);
}
#endif

static sdlahw_t*
sdla_hw_search(u_int16_t atype, int slot_no, int bus_no, int cpu_no)
{
	sdlahw_t*	tmp_hw;

	
	LIST_FOREACH(tmp_hw, &sdlahw_head, next) {
		if (tmp_hw->hwcard == NULL) {
			log(LOG_INFO,
			"Critical Error: sdla_cpu_search: line %d\n",
					__LINE__);
			// XXX REMOVE in LIST_FOREACH
			LIST_REMOVE(tmp_hw, next);
			continue;
		}
		if (tmp_hw->hwcard->atype != atype) {
			// XXX why ???
			LIST_REMOVE(tmp_hw, next);
			continue;
		}
		if (tmp_hw->hwcard->slot_no == slot_no &&
		    tmp_hw->hwcard->bus_no == bus_no &&
		    tmp_hw->cpu_no == cpu_no)
			return (tmp_hw);
	}

	return (NULL);
}


/*
 * Set up adapter.
 * o detect adapter type
 * o set up adapter shared memory
 * Return:	0	ok.
 *		< 0	error
 */

int
sdla_setup(void *phw)
{
	sdlahw_card_t*	hwcard = NULL;
	sdlahw_t*	hw = (sdlahw_t*)phw;
	int		err=0;

	WAN_ASSERT(hw == NULL);
	SDLA_MAGIC(hw);
	WAN_ASSERT(hw->hwcard == NULL);
	hwcard = hw->hwcard;
	switch (hwcard->type) {
	case SDLA_AFT:
		break;

	default:
		log(LOG_INFO, "%s: Invalid card type %x\n",
				hw->devname, hw->hwcard->type);
		return (EINVAL);
	}

	hw->dpmsize = SDLA_WINDOWSIZE;

	err = sdla_detect(hw);
	return (err);
}


/*
 * Shut down SDLA: disable shared memory access and interrupts, stop CPU, etc.
 */
int
sdla_down(void *phw)
{
	sdlahw_card_t*	card = NULL;
	sdlahw_t*	hw = (sdlahw_t*)phw;

	WAN_ASSERT(hw == NULL);
	SDLA_MAGIC(hw);
	WAN_ASSERT(hw->hwcard == NULL);
	card = hw->hwcard;
	switch (card->type) {
	case SDLA_AFT:
		/* free up the allocated virtual memory */
		if (hw->status & SDLA_MEM_MAPPED) {
			bus_space_unmap(hw->hwcard->memt,
					hw->dpmbase,
					XILINX_PCI_MEM_SIZE);
			hw->status &= ~SDLA_MEM_MAPPED;
		}
		break;

	default:
		return (EINVAL);
	}
	return (0);
}

/*
 * Read the hardware interrupt status.
 */
int
sdla_read_int_stat(void *phw, u_int32_t *int_status)
{
	sdlahw_card_t*	card = NULL;
	sdlahw_t*	hw = (sdlahw_t*)phw;

	WAN_ASSERT(hw == NULL);
	SDLA_MAGIC(hw);
	WAN_ASSERT(hw->hwcard == NULL);
	card = hw->hwcard;
	switch (card->type) {
	case SDLA_AFT:
		sdla_pci_read_config_dword(hw, PCI_INT_STATUS, int_status);
	}
	return (0);
}


/*
 * Generate an interrupt to adapter's CPU.
 */
int
sdla_cmd(void *phw, unsigned long offset, wan_mbox_t *mbox)
{
	sdlahw_t	*hw = (sdlahw_t*)phw;
	int		 len = sizeof(wan_cmd_t);
	int		 err = 0;
	u_int8_t	 value;

	SDLA_MAGIC(hw);
	len += mbox->wan_data_len;

	sdla_peek(hw, offset, (void*)&value, 1);
	if (value != 0x00) {
		log(LOG_INFO, "%s: opp flag set on entry to sdla_exec!\n",
				hw->devname);
		return (0);
	}
	mbox->wan_opp_flag = 0x00;
	sdla_poke(hw, offset, (void*)mbox, len);

	err = sdla_exec(hw, offset);
	if (!err) {
		log(LOG_INFO, "%s: Command 0x%02X failed!\n",
					hw->devname, mbox->wan_command);
		return (WAN_CMD_TIMEOUT);
	}
	sdla_peek(hw, offset, (void*)mbox, sizeof(wan_cmd_t));
	if (mbox->wan_data_len) {
		sdla_peek(hw, offset+offsetof(wan_mbox_t, wan_data),
		    mbox->wan_data, mbox->wan_data_len);
	}

	return (mbox->wan_return_code);
}

/*
 * Execute Adapter Command.
 * o Set exec flag.
 * o Busy-wait until flag is reset.
 * o Return number of loops made, or 0 if command timed out.
 */
static int
sdla_exec(sdlahw_t *hw, unsigned long offset)
{
	volatile unsigned long	tstop;
	volatile unsigned long	nloops;
	u_int8_t		value;

	value = 0x01;
	sdla_poke(hw, offset, (void*)&value, 1);
	tstop = ticks + EXEC_TIMEOUT;

	sdla_peek(hw, offset, (void*)&value, 1);
	for (nloops = 1; value == 0x01; ++ nloops) {
		DELAY(EXEC_DELAY);
		if (ticks > tstop || nloops > MAX_NLOOPS) {
			log(LOG_INFO, "%s: Timeout %lu ticks (max=%lu) "
			    "loops %lu (max=%u)\n", hw->devname,
			    (ticks-tstop+EXEC_TIMEOUT),
			    (unsigned long)EXEC_TIMEOUT, nloops, MAX_NLOOPS);
			return (0);		/* time is up! */
		}
		sdla_peek(hw, offset, (void*)&value, 1);
	}

	return (nloops);
}


/*
 * Read absolute adapter memory.
 * Transfer data from adapter's memory to data buffer.
 *
 * Note:
 * Care should be taken when crossing dual-port memory window boundary.
 * This function is not atomic, so caller must disable interrupt if
 * interrupt routines are accessing adapter shared memory.
 */
int
sdla_peek(void *phw, unsigned long addr, void *buf, unsigned len)
{
	sdlahw_card_t*	card = NULL;
	sdlahw_t*	hw = (sdlahw_t*)phw;
	int err = 0;

	WAN_ASSERT(hw == NULL);
	SDLA_MAGIC(hw);
	WAN_ASSERT(hw->hwcard == NULL);
	card = hw->hwcard;
	if (addr + len > hw->memory)	/* verify arguments */
		return (EINVAL);

	switch (card->type) {
	case SDLA_AFT:
		sdla_peek_by_4(hw, addr, buf, len);
		break;

	default:
		log(LOG_INFO, "%s: Invalid card type 0x%X\n",
			__FUNCTION__,card->type);
		err = (EINVAL);
		break;
	}
	return (err);
}


/*
 * Read data from adapter's memory to a data buffer in 4-byte chunks.
 * Note that we ensure that the SDLA memory address is on a 4-byte boundary
 * before we begin moving the data in 4-byte chunks.
*/
static void
sdla_peek_by_4(sdlahw_t *hw, unsigned long offset, void *buf, unsigned int len)
{
	/* byte copy data until we get to a 4-byte boundary */
	while (len && (offset & 0x03)) {
		sdla_bus_read_1(hw, offset++, (u_int8_t*)buf);
		((u_int8_t *)buf)++;
		len--;
	}

	/* copy data in 4-byte chunks */
	while (len >= 4) {
		sdla_bus_read_4(hw, offset, (u_int32_t*)buf);
		(u_int8_t*)buf += 4;
		offset += 4;
		len -= 4;
	}

	/* byte copy any remaining data */
	while (len) {
		sdla_bus_read_1(hw, offset++, (u_int8_t*)buf);
		((u_int8_t *)buf)++;
		len--;
	}
}

/*
 * Write Absolute Adapter Memory.
 * Transfer data from data buffer to adapter's memory.
 *
 * Note:
 * Care should be taken when crossing dual-port memory window boundary.
 * This function is not atomic, so caller must disable interrupt if
 * interrupt routines are accessing adapter shared memory.
 */
int
sdla_poke(void *phw, unsigned long addr, void *buf, unsigned len)
{
	sdlahw_card_t*	card = NULL;
	sdlahw_t*	hw = (sdlahw_t*)phw;
	int err = 0;

	WAN_ASSERT(hw == NULL);
	SDLA_MAGIC(hw);
	WAN_ASSERT(hw->hwcard == NULL);
	card = hw->hwcard;
	if (addr + len > hw->memory) {	/* verify arguments */
		return (EINVAL);
	}

	switch (card->type) {
	case SDLA_AFT:
		sdla_poke_by_4(hw, addr, buf, len);
		break;

	default:
		log(LOG_INFO, "%s: Invalid card type 0x%X\n",
			__FUNCTION__,card->type);
		err = (EINVAL);
		break;
	}
	return (err);
}


/*
 * Write from a data buffer to adapter's memory in 4-byte chunks.
 * Note that we ensure that the SDLA memory address is on a 4-byte boundary
 * before we begin moving the data in 4-byte chunks.
*/
static void
sdla_poke_by_4(sdlahw_t *hw, unsigned long offset, void *buf, unsigned int len)
{
	/* byte copy data until we get to a 4-byte boundary */
	while (len && (offset & 0x03)) {
		sdla_bus_write_1(hw, offset++, *(char *)buf);
		((char *)buf) ++;
		len --;
	}

	/* copy data in 4-byte chunks */
	while (len >= 4) {
		sdla_bus_write_4(hw, offset, *(unsigned long *)buf);
		offset += 4;
		(char*)buf += 4;
		len -= 4;
	}

	/* byte copy any remaining data */
	while (len) {
		sdla_bus_write_1(hw, offset++, *(char *)buf);
		((char *)buf) ++;
		len --;
	}
}

int
sdla_poke_byte(void *phw, unsigned long offset, u_int8_t value)
{
	sdlahw_t *hw = (sdlahw_t*)phw;

	SDLA_MAGIC(hw);
	/* Sangoma ISA card sdla_bus_write_1(hw, offset, value); */
	sdla_poke(hw, offset, (void*)&value, 1);
	return (0);
}

int
sdla_set_bit(void *phw, unsigned long offset, u_int8_t value)
{
	sdlahw_t	*hw = (sdlahw_t*)phw;
	u_int8_t	 tmp;

	SDLA_MAGIC(hw);
	/* Sangoma ISA card -> sdla_bus_read_1(hw, offset, &tmp); */
	sdla_peek(hw, offset, (void*)&tmp, 1);
	tmp |= value;
	/* Sangoma ISA card -> sdla_bus_write_1(hw, offset, tmp); */
	sdla_poke(hw, offset, (void*)&tmp, 1);
	return (0);
}

int
sdla_clear_bit(void *phw, unsigned long offset, u_int8_t value)
{
	sdlahw_t	*hw = (sdlahw_t*)phw;
	u_int8_t	 tmp;

	SDLA_MAGIC(hw);
	/* Sangoma ISA card -> sdla_bus_read_1(hw, offset, &tmp); */
	sdla_peek(hw, offset, (void*)&tmp, 1);
	tmp &= ~value;
	/* Sangoma ISA card -> sdla_bus_write_1(hw, offset, tmp); */
	sdla_poke(hw, offset, (void*)&tmp, 1);
	return (0);
}

/*
 * Find the AFT HDLC PCI adapter in the PCI bus.
 * Return the number of AFT adapters found (0 if no adapter found).
 */
static int
sdla_detect_aft(sdlahw_t *hw)
{
	sdlahw_card_t	*card;
	u_int16_t	 ut_u16;

	WAN_ASSERT(hw == NULL);
	WAN_ASSERT(hw->hwcard == NULL);
	card = hw->hwcard;
	sdla_pci_read_config_dword(hw,
	    (hw->cpu_no == SDLA_CPU_A) ? PCI_IO_BASE_DWORD :
	    PCI_MEM_BASE0_DWORD, (u_int32_t*)&hw->mem_base_addr);
	if (!hw->mem_base_addr) {
		if (hw->cpu_no == SDLA_CPU_B) {
			printf("%s: No PCI memory allocated for CPU #B\n",
					hw->devname);
		} else {
			printf("%s: No PCI memory allocated to card\n",
					hw->devname);
		}
		return (EINVAL);
	}
#ifdef DEBUG
	log(LOG_INFO,  "%s: AFT PCI memory at 0x%lX\n",
				hw->devname, (unsigned long)hw->mem_base_addr);
#endif /* DEBUG */
	sdla_pci_read_config_byte(hw, PCI_INTLINE, (u_int8_t*)&hw->irq);
	if (hw->irq == PCI_IRQ_NOT_ALLOCATED) {
		printf("%s: IRQ not allocated to AFT adapter\n", hw->devname);
		return (EINVAL);
	}

#ifdef DEBUG
	log(LOG_INFO, "%s: IRQ %d allocated to the AFT PCI card\n",
	    hw->devname, hw->irq);
#endif /* DEBUG */

	hw->memory=XILINX_PCI_MEM_SIZE;

	/* Map the physical PCI memory to virtual memory */
	bus_space_map(hw->hwcard->memt, hw->mem_base_addr, XILINX_PCI_MEM_SIZE,
	    0, &hw->dpmbase);
	if (!hw->dpmbase) {
		printf("%s: couldn't map memory\n", hw->devname);
		return (EINVAL);
	}
	hw->status |= SDLA_MEM_MAPPED;


	/* Enable master operation on PCI and enable bar0 memory */
	sdla_pci_read_config_word(hw, XILINX_PCI_CMD_REG, &ut_u16);
	ut_u16 |=0x06;
	sdla_pci_write_config_word(hw, XILINX_PCI_CMD_REG, ut_u16);

	/* Set PCI Latency of 0xFF*/
	sdla_pci_write_config_dword(hw, XILINX_PCI_LATENCY_REG,
	    XILINX_PCI_LATENCY);

	return (0);
}


/*
 * Detect adapter type.
 */
static int
sdla_detect(sdlahw_t *hw)
{
	sdlahw_card_t	*card = NULL;
	int		 err = 0;

	WAN_ASSERT(hw == NULL);
	WAN_ASSERT(hw->hwcard == NULL);
	card = hw->hwcard;
	switch (card->type) {
	case SDLA_AFT:
		err = sdla_detect_aft(hw);
		break;
	}
	if (err)
		sdla_down(hw);

	return (err);
}

int
sdla_is_te1(void *phw)
{
	sdlahw_card_t	*hwcard = NULL;
	sdlahw_t	*hw = (sdlahw_t*)phw;

	WAN_ASSERT(hw == NULL);
	SDLA_MAGIC(hw);
	WAN_ASSERT(hw->hwcard == NULL);
	hwcard = hw->hwcard;
	switch (hwcard->atype) {
	case S5144_ADPTR_1_CPU_T1E1:
	case S5147_ADPTR_2_CPU_T1E1:
	case S5148_ADPTR_1_CPU_T1E1:
	case A101_ADPTR_1TE1:
	case A101_ADPTR_2TE1:
		return (1);
	}
	return (0);
}

int
sdla_check_mismatch(void *phw, unsigned char media)
{
	sdlahw_card_t	*hwcard = NULL;
	sdlahw_t	*hw = (sdlahw_t*)phw;

	WAN_ASSERT(hw == NULL);
	SDLA_MAGIC(hw);
	WAN_ASSERT(hw->hwcard == NULL);
	hwcard = hw->hwcard;
	if (media == WAN_MEDIA_T1 ||
	    media == WAN_MEDIA_E1) {
		if (hwcard->atype != S5144_ADPTR_1_CPU_T1E1 &&
		    hwcard->atype != S5147_ADPTR_2_CPU_T1E1 &&
		    hwcard->atype != S5148_ADPTR_1_CPU_T1E1) {
			log(LOG_INFO, "%s: Error: Card type mismatch: "
			    "User=T1/E1 Actual=%s\n", hw->devname,
			    SDLA_ADPTR_DECODE(hwcard->atype));
			return (EIO);
		}
		hwcard->atype = S5144_ADPTR_1_CPU_T1E1;

	} else if (media == WAN_MEDIA_56K) {
		if (hwcard->atype != S5145_ADPTR_1_CPU_56K) {
			log(LOG_INFO, "%s: Error: Card type mismatch: "
			    "User=56K Actual=%s\n", hw->devname,
			    SDLA_ADPTR_DECODE(hwcard->atype));
			return (EIO);
		}
	} else {
		if (hwcard->atype == S5145_ADPTR_1_CPU_56K ||
		    hwcard->atype == S5144_ADPTR_1_CPU_T1E1 ||
		    hwcard->atype == S5147_ADPTR_2_CPU_T1E1 ||
		    hwcard->atype == S5148_ADPTR_1_CPU_T1E1) {
			log(LOG_INFO, "%s: Error: Card type mismatch: "
			    "User=S514(1/2/3) Actual=%s\n", hw->devname,
			    SDLA_ADPTR_DECODE(hwcard->atype));
			return (EIO);
		}
	}

	return (0);
}

int
sdla_getcfg(void *phw, int type, void *value)
{
	sdlahw_t*	hw = (sdlahw_t*)phw;
	sdlahw_card_t *hwcard;

	WAN_ASSERT(hw == NULL);
	SDLA_MAGIC(hw);
	WAN_ASSERT(hw->hwcard == NULL);
	hwcard = hw->hwcard;
	switch (type) {
	case SDLA_CARDTYPE:
		*(u_int16_t*)value = hwcard->type;
		break;
	case SDLA_MEMBASE:
		*(bus_space_handle_t*)value = hw->dpmbase;
		break;
	case SDLA_MEMEND:
		*(u_int32_t*)value = ((unsigned long)hw->dpmbase +
		    hw->dpmsize - 1);
		break;
	case SDLA_MEMSIZE:
		*(u_int16_t*)value = hw->dpmsize;
		break;
	case SDLA_MEMORY:
		*(u_int32_t*)value = hw->memory;
		break;
	case SDLA_IRQ:
		*(u_int16_t*)value = hw->irq;
		break;
	case SDLA_ADAPTERTYPE:
		*(u_int16_t*)value = hwcard->atype;
		break;
	case SDLA_CPU:
		*(u_int16_t*)value = hw->cpu_no;
		break;
	case SDLA_SLOT:
		*(u_int16_t*)value = hwcard->slot_no;
		break;
	case SDLA_BUS:
		*(u_int16_t*)value = hwcard->bus_no;
		break;
	case SDLA_DMATAG:
		*(bus_dma_tag_t*)value = hwcard->pa.pa_dmat;
		break;
	case SDLA_PCIEXTRAVER:
		*(u_int8_t*)value = hwcard->pci_extra_ver;
		break;
	case SDLA_BASEADDR:
		*(u_int32_t*)value = hw->mem_base_addr;
		break;
	}
	return (0);
}


int
sdla_get_hwcard(void *phw, void **phwcard)
{
	sdlahw_t *hw = (sdlahw_t*)phw;

	WAN_ASSERT(hw == NULL);
	SDLA_MAGIC(hw);

	*phwcard = hw->hwcard;
	return (0);
}


int
sdla_get_hwprobe(void *phw, void **str)
{
	sdlahw_t *hw = (sdlahw_t*)phw;

	WAN_ASSERT(hw == NULL);
	SDLA_MAGIC(hw);

	if (hw->hwprobe)
		*str = hw->hwprobe->hw_info;

	return (0);
}

int
sdla_bus_write_1(void *phw, unsigned int offset, u_int8_t value)
{
	sdlahw_t *hw = (sdlahw_t*)phw;

	WAN_ASSERT(hw == NULL);
	SDLA_MAGIC(hw);
	if (!(hw->status & SDLA_MEM_MAPPED))
		return (0);
	bus_space_write_1(hw->hwcard->memt, hw->dpmbase, offset, value);
	return (0);
}

int
sdla_bus_write_2(void *phw, unsigned int offset, u_int16_t value)
{
	sdlahw_t *hw = (sdlahw_t*)phw;

	WAN_ASSERT(hw == NULL);
	SDLA_MAGIC(hw);
	if (!(hw->status & SDLA_MEM_MAPPED))
		return (0);
	bus_space_write_2(hw->hwcard->memt, hw->dpmbase, offset, value);
	return (0);
}

int
sdla_bus_write_4(void *phw, unsigned int offset, u_int32_t value)
{
	sdlahw_t *hw = (sdlahw_t*)phw;

	WAN_ASSERT(hw == NULL);
	SDLA_MAGIC(hw);
	if (!(hw->status & SDLA_MEM_MAPPED))
		return (0);
	bus_space_write_4(hw->hwcard->memt, hw->dpmbase, offset, value);
	return (0);
}

int
sdla_bus_read_1(void *phw, unsigned int offset, u_int8_t *value)
{
	sdlahw_t *hw = (sdlahw_t*)phw;

	WAN_ASSERT2(hw == NULL, 0);
	SDLA_MAGIC(hw);
	if (!(hw->status & SDLA_MEM_MAPPED))
		return (0);
	*value = bus_space_read_1(hw->hwcard->memt, hw->dpmbase, offset);
	return (0);
}

int
sdla_bus_read_2(void *phw, unsigned int offset, u_int16_t *value)
{
	sdlahw_t *hw = (sdlahw_t*)phw;

	WAN_ASSERT2(hw == NULL, 0);
	SDLA_MAGIC(hw);
	if (!(hw->status & SDLA_MEM_MAPPED))
		return (0);
	*value = bus_space_read_2(hw->hwcard->memt, hw->dpmbase, offset);
	return (0);
}

int
sdla_bus_read_4(void *phw, unsigned int offset, u_int32_t *value)
{
	sdlahw_t *hw = (sdlahw_t*)phw;

	WAN_ASSERT2(hw == NULL, 0);
	WAN_ASSERT2(hw->dpmbase == 0, 0);
	SDLA_MAGIC(hw);
	if (!(hw->status & SDLA_MEM_MAPPED))
		return (0);
	*value = bus_space_read_4(hw->hwcard->memt, hw->dpmbase, offset);
	return (0);
}

static int
sdla_pci_read_config_dword(void *phw, int reg, u_int32_t *value)
{
	sdlahw_t	*hw = (sdlahw_t*)phw;
	sdlahw_card_t	*hwcard;

	WAN_ASSERT(hw == NULL);
	SDLA_MAGIC(hw);
	WAN_ASSERT(hw->hwcard == NULL);
	hwcard = hw->hwcard;
	*value = pci_conf_read(hwcard->pa.pa_pc, hwcard->pa.pa_tag, reg);
	return (0);
}

static int
sdla_pci_read_config_word(void *phw, int reg, u_int16_t *value)
{
	sdlahw_t	*hw = (sdlahw_t*)phw;
	sdlahw_card_t	*hwcard;
	u_int32_t	 tmp = 0x00;

	WAN_ASSERT(hw == NULL);
	SDLA_MAGIC(hw);
	WAN_ASSERT(hw->hwcard == NULL);
	hwcard = hw->hwcard;
	tmp = pci_conf_read(hwcard->pa.pa_pc, hwcard->pa.pa_tag, reg);
	*value = (u_int16_t)((tmp >> 16) & 0xFFFF);
	return (0);
}

static int
sdla_pci_read_config_byte(void *phw, int reg, u_int8_t *value)
{
	sdlahw_t	*hw = (sdlahw_t*)phw;
	sdlahw_card_t	*hwcard;
	u_int32_t	 tmp = 0x00;

	WAN_ASSERT(hw == NULL);
	SDLA_MAGIC(hw);
	WAN_ASSERT(hw->hwcard == NULL);
	hwcard = hw->hwcard;
	tmp = pci_conf_read(hwcard->pa.pa_pc, hwcard->pa.pa_tag, reg);
	*value = (u_int8_t)(tmp & 0xFF);
	return (0);
}

static int
sdla_pci_write_config_dword(void *phw, int reg, u_int32_t value)
{
	sdlahw_t *hw = (sdlahw_t*)phw;
	sdlahw_card_t *card;

	WAN_ASSERT(hw == NULL);
	SDLA_MAGIC(hw);
	WAN_ASSERT(hw->hwcard == NULL);
	card = hw->hwcard;
	pci_conf_write(card->pa.pa_pc, card->pa.pa_tag, reg, value);
	return (0);
}

static int
sdla_pci_write_config_word(void *phw, int reg, u_int16_t value)
{
	sdlahw_t *hw = (sdlahw_t*)phw;
	sdlahw_card_t *card;

	WAN_ASSERT(hw == NULL);
	SDLA_MAGIC(hw);
	WAN_ASSERT(hw->hwcard == NULL);
	card = hw->hwcard;
	pci_conf_write(card->pa.pa_pc, card->pa.pa_tag, reg, value);
	return (0);
}
