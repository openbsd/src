/*	$NetBSD: ds-tc-conf.c,v 1.5 1996/01/03 20:39:16 jonathan Exp $	*/

/*
 * Copyright (c) 1995 Jonathan Stone
 * All rights reserved.
 */

/*
 * 3MIN and 3MAXPLUS turbochannel slots.
 * The kmin (3MIN) and kn03 (3MAXPLUS) have the same number of slots.
 * We can share one configuration-struct table and use two slot-address
 * tables to handle the fact that the turbochannel slot size and base
 * addresses are different on the two machines.
 * (thankfully the IOASIC subslots are all the same size.)
 */

#define C(x)	((void *)(u_long)x)

struct confargs tc3_devs[4] = {
	/* name	       slot  offset intpri */
	{ "IOCTL   ",     3,  0x0,   -1,   },  /* offset 0x040000 ?*/
	{ NULL, 	  2,  0x0,    2,   },
	{ NULL, 	  1,  0x0,    1,   },
	{ NULL, 	  0,  0x0,    0,   }

};

/* 3MAXPLUS slot addreseses */
static struct tc_slotdesc tc_kn03_slots [4] = {
       	{ KV(KN03_PHYS_TC_0_START), C(0) },  /* slot0 - tc option slot 0 */
	{ KV(KN03_PHYS_TC_1_START), C(1) },  /* slot1 - tc option slot 1 */
	{ KV(KN03_PHYS_TC_2_START), C(2) },  /* slot2 - tc option slot 2 */
	{ KV(KN03_PHYS_TC_3_START), C(3) }   /* slot3 - IO asic on b'board */
};
int tc_kn03_nslots =
    sizeof(tc_kn03_slots) / sizeof(tc_kn03_slots[0]);


/* 3MAXPLUS turbochannel autoconfiguration table */
struct tc_cpu_desc kn03_tc_desc =
{
	tc_kn03_slots, KN03_TC_NSLOTS,
	tc3_devs, KN03_TC_NSLOTS, /*XXX*/
	tc_ds_ioasic_intr_setup,
	tc_ds_ioasic_intr_establish,
	tc_ds_ioasic_intr_disestablish,
	(void*)-1
};

/************************************************************************/

/* 3MIN slot addreseses */
static struct tc_slotdesc tc_kmin_slots [] = {
       	{ KV(KMIN_PHYS_TC_0_START), C(0) },   /* slot0 - tc option slot 0 */
	{ KV(KMIN_PHYS_TC_1_START), C(1) },   /* slot1 - tc option slot 1 */
	{ KV(KMIN_PHYS_TC_2_START), C(3) },   /* slot2 - tc option slot 2 */
	{ KV(KMIN_PHYS_TC_3_START), C(4) }    /* slot3 - IO asic on b'board */
};

int tc_kmin_nslots =
    sizeof(tc_kmin_slots) / sizeof(tc_kmin_slots[0]);

/* 3MIN turbochannel autoconfiguration table */
struct tc_cpu_desc kmin_tc_desc =
{
	tc_kmin_slots,  KMIN_TC_NSLOTS,
	tc3_devs, KMIN_TC_NSLOTS, /*XXX*/
	tc_ds_ioasic_intr_setup,
	tc_ds_ioasic_intr_establish,
	tc_ds_ioasic_intr_disestablish,
	/*kmin_intr*/ (void*) -1
};

/************************************************************************/

/* MAXINE  turbochannel slots  */
struct confargs xine_devs[4] = {
	/* name	       slot  offset intpri  */
	{ "PMAG-DV ",	  3,  0x0,    3,   },	/* xcfb */
	{ "IOCTL   ",  	  2,  0x0,   -1,   },
	{ NULL, 	  1,  0x0,    1,   },
	{ NULL, 	  0,  0x0,    0,   }
};

/* MAXINE slot addreseses */
static struct tc_slotdesc tc_xine_slots [4] = {
       	{ KV(XINE_PHYS_TC_0_START), C(0) },   /* slot 0 - tc option slot 0 */
	{ KV(XINE_PHYS_TC_1_START), C(1) },   /* slot 1 - tc option slot 1 */
		/* physical space for ``slot 2'' is reserved */
	{ KV(XINE_PHYS_TC_3_START), C(8) },   /* slot 2 - IO asic on b'board */
	{ KV(XINE_PHYS_CFB_START), C(-1) }    /* slot 3 - fb on b'board */
};

int tc_xine_nslots =
    sizeof(tc_xine_slots) / sizeof(tc_xine_slots[0]);

struct tc_cpu_desc xine_tc_desc =
{
	tc_xine_slots, XINE_TC_NSLOTS,
	xine_devs, 4, /*XXX*/ 
	tc_ds_ioasic_intr_setup,
	tc_ds_ioasic_intr_establish,
	tc_ds_ioasic_intr_disestablish,
	/*xine_intr*/ (void *) -1
};


/************************************************************************/

#if 0
#define TC_SCSI  "PMAZ-AA "
#define TC_ETHER "PMAD-AA "
#else
#define TC_SCSI NULL
#define TC_ETHER NULL
#endif

/* 3MAX (kn02) turbochannel slots  */
struct confargs kn02_devs[8] = {
   /* The 3max  supposedly has "KN02    " at 0xbffc0410 */

	/* name        slot  offset intpri  */
	{ KN02_ASIC_NAME, 7,  0x0,   -1,   },	/* System CSR and subslots */
	{ TC_ETHER, 	  6,  0x0,    6,   },	/* slot 6: Ether on cpu board*/
	{ TC_SCSI, 	  5,  0x0,    5,   },	/* slot 5: SCSI on cpu board */
/*XXX*/	{ NULL, 	  4,   -1,    0,   },	/* slot 3 reserved */
/*XXX*/	{ NULL, 	  3,   -1,    0,   },	/* slot 3 reserved */
	{ NULL, 	  2,  0x0,    2,   },	/* slot 2 - TC option slot 2 */
	{ NULL, 	  1,  0x0,    1,   },	/* slot 1 - TC option slot 1 */
	{ NULL, 	  0,  0x0,    0,   }	/* slot 0 - TC option slot 0 */
};

/* slot addreseses */
static struct tc_slotdesc tc_kn02_slots [8] = {
       	{ KV(KN02_PHYS_TC_0_START), },	/* slot 0 - tc option slot 0 */
	{ KV(KN02_PHYS_TC_1_START), },	/* slot 1 - tc option slot 1 */
	{ KV(KN02_PHYS_TC_2_START), },	/* slot 2 - tc option slot 2 */
	{ KV(KN02_PHYS_TC_3_START), },	/* slot 3 - reserved */
	{ KV(KN02_PHYS_TC_4_START), },	/* slot 4 - reserved */
	{ KV(KN02_PHYS_TC_5_START), },	/* slot 5 - SCSI on cpu board */
	{ KV(KN02_PHYS_TC_6_START), },	/* slot 6 - Ether on cpu board */
	{ KV(KN02_PHYS_TC_7_START), }	/* slot 7 - system devices */

};

int tc_kn02_nslots =
    sizeof(tc_kn02_slots) / sizeof(tc_kn02_slots[0]);

#define KN02_ROM_NAME KN02_ASIC_NAME

#define TC_KN02_DEV_IOASIC     -1
#define TC_KN02_DEV_ETHER	6
#define TC_KN02_DEV_SCSI	5

struct tc_builtin tc_kn02_builtins[] = {
	{ KN02_ROM_NAME,7, 0x00000000, C(TC_KN02_DEV_IOASIC),	},
	{ TC_ETHER,	6, 0x00000000, C(TC_KN02_DEV_ETHER),	},
	{ TC_SCSI,	5, 0x00000000, C(TC_KN02_DEV_SCSI),	},
};


struct tc_cpu_desc kn02_tc_desc =
{
	tc_kn02_slots, KN02_TC_NSLOTS,
	kn02_devs, 8, /*XXX*/
	tc_ds_ioasic_intr_setup,
	tc_ds_ioasic_intr_establish,
	tc_ds_ioasic_intr_disestablish,
	/*kn02_intr*/ (void*) -1 
};
