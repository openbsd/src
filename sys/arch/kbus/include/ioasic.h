struct ioasic_reg
{
  volatile int ioasic_ir;	/* 0x00: ioasic interrupt register.  */
#define IOASIC_IR_VMEPANIC	0x01	/* VMEbus panic condition.  */
#define IOASIC_IR_SI_PO		0x02	/* scsi page overflow.  */
#define IOASIC_IR_SI_ECC_TO	0x04	/* scsi ECC/timeout error.  */

  volatile int ioasic_ei_rar;	/* 0x04: ioasic enet recv addr.  */
#define IOASIC_EI_RAR_SHIFT	15

  volatile int ioasic_ei_tar1;	/* 0x08: ioasic enet xmit addr reg 1.  */
  volatile int ioasic_ei_tar2;	/* 0x0C: ioasic enet xmit addr reg 2.  */
#define IOASIC_EI_TAR_SHIFT	13

  /* As far as I know, the IOASIC is the dma controller.  The address
     is split between SAR (the 19 high bits) and CTL (the 13 low bits).
     CTL also contains the sens of transfer.  */
  volatile int ioasic_si_sar;	/* 0x10: ioasic right shift addr.  */
#define IOASIC_SI_SAR_SHIFT	13
#define IOASIC_SI_SAR_MASK	0x0007FFFF	/* 19 bits.  */
  volatile int ioasic_si_ctl;	/* 0x14: ioasic scsi control register.  */
#define IOASIC_SI_CTL_READ	0
#define IOASIC_SI_CTL_WRITE	1
#define IOASIC_SI_CTL_PRE	2
#define IOASIC_SI_CTL_SHIFT	2	/* left shift addr.  */
#define IOASIC_SI_CTL_MASK	0x00007FFF
#define IOASIC_SI_CTL_AMASK	0x00007FFC	/* 13 bits.  */
  int ioasic_pad[6];
  volatile int ioasic_si_sar_pre; /* 0x30: prefetch ioasic scsi addr reg.  */
};

#define IOASIC_ADDR	0x90000000
#define IOASIC_SIZE	0x2000
#define WD33C93A_ADDR	0x90000800
#define WD33C93A_OFFSET	0x00000800
#define LANCE_ADDR	0X90001000
#define LANCE_OFFSET	0x00001000

extern struct ioasic_reg *ioasic;
