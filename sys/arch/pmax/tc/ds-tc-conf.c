/*	$NetBSD: ds-tc-conf.c,v 1.4 1995/10/09 01:45:28 jonathan Exp $	*/

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
struct confargs tc3_devs[4] = {
	/* name	      entry   pri xxx */
	{ "IOCTL   ",     3, -1,  /*0x040000*/ 0x0,   },
	{ NULL, 	  2,  2,    0x0,   },
	{ NULL, 	  1,  1,    0x0,   },
	{ NULL, 	  0,  0,    0x0,   }

};

/* 3MAXPLUS slot addreseses */
 static struct tc_slot_desc kn03_slot_addrs [4] = {
       	{ KV(KN03_PHYS_TC_0_START), },	/* slot 0 - tc option slot 0 */
	{ KV(KN03_PHYS_TC_1_START), },	/* slot 1 - tc option slot 1 */
	{ KV(KN03_PHYS_TC_2_START), },	/* slot 2 - tc option slot 2 */
	{ KV(KN03_PHYS_TC_3_START), }	/* slot 3 - IOCTL asic on CPU board */
};

/* 3MAXPLUS turbochannel autoconfiguration table */
struct tc_cpu_desc kn03_tc_desc =
{
	kn03_slot_addrs, KN03_TC_NSLOTS,
	tc3_devs, KN03_TC_NSLOTS, /*XXX*/
	tc_ds_ioasic_intr_setup,
	tc_ds_ioasic_intr_establish,
	tc_ds_ioasic_intr_disestablish,
	(void*)-1
};

/************************************************************************/

/* 3MIN slot addreseses */
static struct tc_slot_desc kmin_slot_addrs [4] = {
       	{ KV(KMIN_PHYS_TC_0_START), },	/* slot 0 - tc option slot 0 */
	{ KV(KMIN_PHYS_TC_1_START), },	/* slot 1 - tc option slot 1 */
	{ KV(KMIN_PHYS_TC_2_START), },	/* slot 2 - tc option slot 2 */
	{ KV(KMIN_PHYS_TC_3_START), }	/* slot 3 - IOCTL asic on CPU board */
};

/* 3MIN turbochannel autoconfiguration table */
struct tc_cpu_desc kmin_tc_desc =
{
	kmin_slot_addrs, KMIN_TC_NSLOTS,
	tc3_devs, KMIN_TC_NSLOTS, /*XXX*/
	tc_ds_ioasic_intr_setup,
	tc_ds_ioasic_intr_establish,
	tc_ds_ioasic_intr_disestablish,
	/*kmin_intr*/ (void*) -1
};

/************************************************************************/

/* MAXINE  turbochannel slots  */
struct confargs xine_devs[4] = {
	{ "PMAG-DV ",	  3,   3,    0x0,  },	/* xcfb ? */
	{ "IOCTL   ",  	  2,  -1,    0x0,  },
	{ NULL, 	  1,   1,    0x0,  },
	{ NULL, 	  0,   0,    0x0,  }
};

/* MAXINE slot addreseses */
static struct tc_slot_desc xine_slot_addrs [4] = {
       	{ KV(XINE_PHYS_TC_0_START), },	/* slot 0 - tc option slot 0 */
	{ KV(XINE_PHYS_TC_1_START), },	/* slot 1 - tc option slot 1 */
	{ KV(XINE_PHYS_TC_3_START), },	/* slot 2 - IOCTL asic on CPU board */
	{ KV(XINE_PHYS_CFB_START),  }	/* slot 3 - fb on CPU board */
};

struct tc_cpu_desc xine_tc_desc =
{
	xine_slot_addrs, XINE_TC_NSLOTS,
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

	/* name	     entry   pri xxx */
	{ KN02_ASIC_NAME, 7,  -1,   0x0,   },	/* System CSR and subslots */
	{ TC_ETHER, 	  6,   6,   0x0,   },	/* slot 6: Ether on cpu board*/
	{ TC_SCSI, 	  5,   5,   0x0,   },	/* slot 5: SCSI on cpu board */
/*XXX*/	{ NULL, 	  4,   0,    -1,   },	/* slot 3 reserved */
/*XXX*/	{ NULL, 	  3,   0,    -1,   },	/* slot 3 reserved */
	{ NULL, 	  2,   2,   0x0,   },	/* slot 2 - TC option slot 2 */
	{ NULL, 	  1,   1,   0x0,   },	/* slot 1 - TC option slot 1 */
	{ NULL, 	  0,   0,   0x0,   }	/* slot 0 - TC option slot 0 */
};

/* slot addreseses */
static struct tc_slot_desc kn02_slot_addrs [8] = {
       	{ KV(KN02_PHYS_TC_0_START), },	/* slot 0 - tc option slot 0 */
	{ KV(KN02_PHYS_TC_1_START), },	/* slot 1 - tc option slot 1 */
	{ KV(KN02_PHYS_TC_2_START), },	/* slot 2 - tc option slot 2 */
	{ KV(KN02_PHYS_TC_3_START), },	/* slot 3 - reserved */
	{ KV(KN02_PHYS_TC_4_START), },	/* slot 4 - reserved */
	{ KV(KN02_PHYS_TC_5_START), },	/* slot 5 - SCSI on cpu board */
	{ KV(KN02_PHYS_TC_6_START), },	/* slot 6 - Ether on cpu board */
	{ KV(KN02_PHYS_TC_7_START), }	/* slot 7 - system devices */

};


struct tc_cpu_desc kn02_tc_desc =
{
	kn02_slot_addrs, KN02_TC_NSLOTS,
	kn02_devs, 8, /*XXX*/
	tc_ds_ioasic_intr_setup,
	tc_ds_ioasic_intr_establish,
	tc_ds_ioasic_intr_disestablish,
	/*kn02_intr*/ (void*) -1 
};
