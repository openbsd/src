#include <dev/ic/z8530reg.h>
#include <machine/prom.h>
#include <machine/asm.h>

void udelay (unsigned long usec);

static unsigned char *zs0_a = (unsigned char *) 0x17012020;
static unsigned char *zs0_b = (unsigned char *) 0x17012000;
static unsigned char *zs_ms = (unsigned char *) 0x17011020;
static unsigned char *zs_kb = (unsigned char *) 0x17011000;

void
zs_reg_write (unsigned char *zs_addr, unsigned char reg, unsigned char val)
{
  *zs_addr = reg;
  udelay (2);
  *zs_addr = val;
}

unsigned char
zs_reg_read (unsigned char *zs_addr, unsigned char reg)
{
  *zs_addr = reg;
  udelay (2);
  return *zs_addr;
}

void
zs0_putc (char c)
{
  unsigned char r0;

  /* Wait until Tx is free.  */
  do
    {
      *zs0_a = 0;
      udelay (2);
    }  
  while ((*zs0_a & ZSRR0_TX_READY) == 0);
 
  /* Write the character.  */
  zs0_a[0x10] = c;
  udelay (2);

  /* Wait until transmission.  */
  do
    {
      *zs0_a = 0;
      udelay (2);
    }  
  while ((*zs0_a & ZSRR0_TX_READY) == 0);
}

void
putchar (char c)
{
  if (c == '\n')
    zs0_putc ('\r');
  zs0_putc (c);
}

int
getchar (void)
{
  unsigned char r0;

  /* Wait until Rx is free.  */
  do
    {
      *zs0_a = 0;
      udelay (2);
    }  
  while ((*zs0_a & ZSRR0_RX_READY) == 0);
 
  /* Read the character.  */
  return zs0_a[0x10];
}

void
udelay (unsigned long usec)
{
  register int i;

  while (usec--)
    for (i = 20; i; i--)
      ;
}


static void
zs_loadreg (char *zsc, char *reg)
{
 /* Reset int.  */
  zs_reg_write (zsc, 0, ZSM_RESET_STINT);
  zs_reg_write (zsc, 0, ZSM_RESET_TXINT);
  zs_reg_write (zsc, 0, ZSM_RESET_ERR);
  zs_reg_write (zsc, 0, ZSM_RESET_IUS);

  /* Set vector.  */
  zs_reg_write (zsc, 2, reg[2]);

  zs_reg_write (zsc, 3, reg[3] & ~ZSWR3_RX_ENABLE);

  zs_reg_write (zsc, 4, reg[4]);

  zs_reg_write (zsc, 5, reg[5] & ~ZSWR5_TX_ENABLE);

  zs_reg_write (zsc, 9, reg[9]);
  
  zs_reg_write (zsc, 10, reg[10]);

  zs_reg_write (zsc, 11, reg[11]);
  zs_reg_write (zsc, 12, reg[12]);
  zs_reg_write (zsc, 13, reg[13]); 
  zs_reg_write (zsc, 14, reg[14]);
  zs_reg_write (zsc, 15, reg[15]);

  zs_reg_write (zsc, 3, reg[3]);
  zs_reg_write (zsc, 5, reg[5]);
  zs_reg_write (zsc, 1, reg[1]);
}

void
set_zs (char *zsc)
{
  static  char regs[16]=
    {
      0,	/* 0 */
      /* ZSWR1_RIE | */ ZSWR1_SIE,
      64,	/* Vector.  */
      ZSWR3_RX_8 | ZSWR3_RX_ENABLE,
      ZSWR4_CLK_X16 | ZSWR4_ONESB,
      ZSWR5_TX_8 | ZSWR5_TX_ENABLE,
      0,
      0,
      0,
      ZSWR9_MASTER_IE, /* 9 */
      0,
      ZSWR11_RXCLK_BAUD | ZSWR11_TXCLK_BAUD,
      10,		/* 12: baud rate.  */
      0,
      ZSWR14_BAUD_FROM_PCLK | ZSWR14_BAUD_ENA,
      ZSWR15_BREAK_IE
    };
  zs_loadreg (zsc, regs);
  zs_reg_write (zsc, 0, ZSWR0_CLR_INTR);
}

void
set_kbd_zs (char *zsc)
{
  static  char regs[16]=
    {
      0,	/* 0 */
      0, /* ZSWR1_RIE | ZSWR1_SIE, */
      64,	/* Vector.  */
      ZSWR3_RX_8 | ZSWR3_RX_ENABLE,
      ZSWR4_CLK_X1 | ZSWR4_ONESB | ZSWR4_PARENB,
      ZSWR5_TX_8 | ZSWR5_TX_ENABLE,
      0,
      0,
      0,
      ZSWR9_MASTER_IE, /* 9 */
      0,
      ZSWR11_RXCLK_RTXC | ZSWR11_TXCLK_RTXC
	| ZSWR11_TRXC_OUT_ENA | ZSWR11_TRXC_BAUD,
#ifdef HZ_100
      /* HZ = 100 */
      0xd2,		/* 12: baud rate.  */
	0x30,
#else
	/* HZ = 60 */
	0x5c,
	0x51,
#endif
	ZSWR14_BAUD_FROM_PCLK | ZSWR14_BAUD_ENA,
	0
    };
  zs_loadreg (zsc, regs);
  zs_reg_write (zsc, 0, ZSWR0_CLR_INTR);
}

void
disp_zs0_b (void)
{
  int brg;

  set_zs (zs0_a);
  brg = ((zs_reg_read (zs0_a, 13) & 0xff) << 8)
    | (zs_reg_read (zs0_a, 12) & 0xff);
  printf ("brg = %d\n", brg);
  printf ("clock: %d\n", (brg + 2) * 2 * 9600 * 16);
}

void
init_kbd (void)
{
  set_kbd_zs (zs_kb);
}

void
zs_intr (void)
{
  char rr3 = zs_reg_read (zs0_a, 3);
#if 1
  printf ("ZS status: 0x%x, pending: 0x%x\n",
	  zs_reg_read (zs0_a, 0), rr3);
#else
  putchar ('I');
#endif
#if 0
  /* Ack it.  */
  if (rr3 & ZSRR3_IP_A_RX)
    printf ("char: 0x%x\n", zs0_a[0x10]);
#endif
  zs_reg_write (zs0_a, 0, ZSWR0_RESET_STATUS);
  zs_reg_write (zs0_a, 0, ZSWR0_CLR_INTR);
}
