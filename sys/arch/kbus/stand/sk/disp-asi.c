#include <stdio.h>
#include <sys/types.h>
#include <kbus/mmu.h>
#include <kbus/pte.h>
#include <kbus/kbus.h>
#include <kbus/led.h>
#include "sparc-asm.h"
#include "idprom.h"
#include "eeprom.h"
#include "prom.h"

#define NOFAULT_CONTINUE	0x01
#define NOFAULT_QUIET		0x02

extern char edata, end;
extern daddr_t debug_tbr, debug_ret;
extern int nofault;
extern char exception_table;

void
set_led (char val)
{
  static const xtab[] = 
    {
      LED_0, LED_1, LED_2, LED_3, 
      LED_4, LED_5, LED_6, LED_7, 
      LED_8, LED_9, LED_A, LED_b, 
      LED_C, LED_d, LED_E, LED_F
    };
  write_asi_half (ASI_LED, 0, (LED_U << 8) | xtab[val & 0x0f]);
}

void
set_debug (void)
{
  char *old_tbr = (char *)(debug_tbr & 0xfffff000UL);
  char *new_tbr = &exception_table;

#define COPY_VECTOR(n) memcpy (new_tbr + n * 16, old_tbr + n * 16, 16)
  COPY_VECTOR (0x1f);
  COPY_VECTOR (0xf0);
}

void
disp (int n)
{
  if (n > 0)
    disp (n - 1);
  printf ("In disp %d\n", n);
}

void
dump_dg (void)
{
  int i, j;
  unsigned char buf[16];

  for (i = 0; i < 2048; i += 16)
    {
      printf ("%03x: ", i);
      for (j = 0; j < 16; j++)
	{
	  buf[j] = read_asi_byte (ASI_DIAG_RAM, (i + j) << 2);
	  printf ("%02x ", buf[j]);
	}
      for (j = 0; j < 16; j++)
	if (buf[j] >= 32 && buf[j] <= 127)
	  putchar (buf[j]);
	else
	  putchar ('.');
      putchar ('\n');
    }
}


void
uninitialize_dg (void)
{
  unsigned int val;

  write_asi_byte (ASI_DIAG_RAM, 16 << 2, 0);
  val = read_asi (ASI_MMCR, 0);
  /* As a cold start and enable watch-dog.  */
  val |= MMCR_CS /* | MMCR_WDEN */ ;
  write_asi (ASI_MMCR, 0, val);
}

void
initialize_dg (void)
{
  unsigned int val;

  write_asi_byte (ASI_DIAG_RAM, 16 << 2, 'G');
  val = read_asi (ASI_MMCR, 0);
  val &= ~MMCR_CS;
  write_asi (ASI_MMCR, 0, val);
}

void
udelay (unsigned long usec)
{
  register int i;

  while (usec--)
    for (i = 20; i; i--)
      ;
}

void
play_with_led (void)
{
  int i, j;

  for (i = 0; i < 10; i++)
    for (j = 0; j < 16; j++)
      {
	set_led (j);
	ta (j);
	udelay (100000);
      }
}

void
disp_pte (unsigned long pte)
{
  printf ("%08x ", pte);
  if (pte & PG_IO)
    printf ("io ");
  if (pte & PG_V)
    printf ("v ");
}

void
dump_pde (unsigned long *pdba)
{
  int i,j;
  unsigned long pte;
  unsigned long *ptp;
  unsigned long pt;
  unsigned long *optp;
  
  for (i = 0; i < NPDEPERPDT; i++)
    {
      printf ("(%03x) 0x%08x 0x%08x  ", i, pdba[i * 2], pdba[i * 2 + 1]);
      if (i & 1)
	printf ("\n");
    }

#if 1
  optp = 0;
  for (i = 0; i < NPDEPERPDT; i++)
    {
      ptp = (unsigned long *)pdba[2 * i];
      printf ("PT # %02x at 0x%08x\n", i, (unsigned long) ptp);
      if (ptp == optp)
	continue;
      if (((unsigned long) ptp & 0xff000000) != 0xff000000)
	continue;
      optp = ptp;
      for (j = 0; j < NPTEPERPT; j++)
	if (ptp[j] != 0)
	  goto not_null;
      printf ("--- all entries are null\n");
      continue;
    not_null:
      for (j = 0; j < NPTEPERPT; j++)
	{
	  printf ("%04x: ", j);
	  disp_pte (ptp[j]);
	  printf ("\n");
	}
      printf ("\n");
    }
#endif
}

void
dump_dtlb (void)
{
  unsigned long i;
  unsigned long pte;

  for (i = 0xff000000UL; i != 0xff100000; i += 0x00002000)
    {
      pte = read_asi (ASI_GTLB_RDD, i);
      printf ("dtlb for 0x%08x: ", i);
      disp_pte (pte);
      printf ("\n");
    }
}

void
dump_itlb (void)
{
  unsigned long i;
  unsigned long pte;

  for (i = 0xfe000000UL; i != 0; i += 0x00001000)
    {
      pte = read_asi (ASI_GTLB_RDI, i);
      printf ("itlb for 0x%08x: ", i);
      disp_pte (pte);
      printf ("\n");
    }
}

#define MASK 0xffffe000
void
disp_dmap (void)
{
  unsigned long i;
  unsigned long map;
  unsigned long pte;

  map = 0;
  for (i = 0x00002000UL; i != 0; i += 0x00002000)
    {
      pte = read_asi (ASI_GTLB_RDD, i);
      if ((pte & MASK) == map + i)
	continue;
      printf (" - 0x%08x\n", i);
      map = (pte & MASK) - i;
      printf ("Mapped at 0x%08x : 0x%08x", pte, i);
    }
}

void
try_dtlb (void)
{
  unsigned long addr = 0xffe00000;
  unsigned long val;
  unsigned long nval[16];
  int i;

  val = read_asi (ASI_GTLB_RDD, addr);
  printf ("DTLB: addr = 0x%08x, val = 0x%08x\n", addr, val);
  i = *(int *)addr;
  printf ("i = %d...", i);
  write_asi (ASI_GTLB_VALD, addr, 0);
  i = *(int *)addr;
  printf ("i = %d\n", i);
  for (i = 1; i < 16; i++)
    nval[i] = read_asi (ASI_GTLB_RDD, addr + i * 0x1000);
  nval[0] = read_asi (ASI_GTLB_RDD, addr);
  write_asi (ASI_GTLB_VALD, addr, val);
  for (i = 0; i < 16; i++)
    printf ("DTLB: addr = 0x%08x, nval[%d] = 0x%08x, val = 0x%08x\n",
	    addr, i, nval[i], read_asi (ASI_GTLB_RDD, addr + i * 0x1000));
}

void
dump_page (int pg)
{
  int i;
  unsigned long *addr = (unsigned long *)0xff000000;

  for (i = 0; i < 2048; i++)
    {
      printf ("0x%04x: 0x%08x ", i, addr[i]);
      if ((i & 3) == 3)
	printf ("\n");
    }
}

int
disp_prom_string (unsigned char *ptr, int maxlen)
{
  int i;
  char c;

  for (i = 0; i < maxlen; i++)
    {
      c = ptr[i * ID_BYTE_OFFSET];
      if (c == 0)
	return i + 1;
      putchar (c);
    }
  return maxlen;
}

void
probe_kbus (void)
{
  unsigned char *pa;
  unsigned char *va;
  unsigned char *p;
  unsigned char c;
  unsigned char id;
  unsigned int offset;
  unsigned int size;
  unsigned int nbr_bytes_read;
  int i, j;

  for (i = 1; i < 8; i++)
    {
      printf ("Slot %d: ", i);
      fflush (stdout);
      pa = (char *)MK_IOADDR (SPACE_ID, i, 0);
      va = (unsigned char *) alloc_page ((daddr_t) pa, PG_IO | PG_V);
    again:
      nofault = NOFAULT_QUIET | NOFAULT_CONTINUE;
      id = va[0];
      if (nofault == 0)
	{
	  printf ("-\n");
	  continue;
	}
      printf ("id: 0x%02x -%c-", id, id & ID_MASK);
#if 0
      for (j = 1; j < 8; j++)
	{
	  nofault = NOFAULT_QUIET | NOFAULT_CONTINUE;
	  c = va[j];
	  if (nofault != 0)
	    printf (", [%d] = 0x%02x", j, c);
	}
#endif

      printf (" Minor: %d, rev: %c%c",
	      va[ID_OFF_MINOR], va[ID_OFF_REVLEV1], va[ID_OFF_REVLEV2]);
      offset = ID_4BYTES_TO_LONG (va[ID_OFF_OFFSET1], va[ID_OFF_OFFSET2],
				  va[ID_OFF_OFFSET3], va[ID_OFF_OFFSET4]);
      size = ID_4BYTES_TO_LONG (va[ID_OFF_SIZE1], va[ID_OFF_SIZE2],
				 va[ID_OFF_SIZE3], va[ID_OFF_SIZE4]);
      printf (" offset: %ld, size: %ld", offset, size);
      nbr_bytes_read = 13;
      putchar (' ');
      nbr_bytes_read += disp_prom_string (va + ID_OFF_STRING,
					    ID_MAX_STRING_SIZE);
      p = va + nbr_bytes_read * ID_BYTE_OFFSET;
      printf ("\n");
      if (nbr_bytes_read == size)
	printf ("All bytes read\n");
      else
	{
	  printf ("%d bytes to read\n", size - nbr_bytes_read);
	  switch (id & ID_MASK)
	    {
	    case ID_MEMORY_BOARD:
	      printf ("Size: %d Mb\n", *p);
	      break;
	    case ID_SYSTEM_BOARD + 1:
	      printf ("Hostid: %lx\n",
		      ID_4BYTES_TO_LONG (p[ID_S_OFF_HOSTID1],
					 p[ID_S_OFF_HOSTID2], 
					 p[ID_S_OFF_HOSTID3], 
					 p[ID_S_OFF_HOSTID4]));
	      printf ("Serial: ");
	      disp_prom_string (p + ID_S_OFF_SERIAL, ID_S_SERIAL_SIZE);
	      printf ("\nEnet addr: %x:%x:%x:%x:%x:%x\n",
		      p[ID_S_OFF_ENETADDR],
		      p[ID_S_OFF_ENETADDR + 1 * ID_BYTE_OFFSET],
		      p[ID_S_OFF_ENETADDR + 2 * ID_BYTE_OFFSET],
		      p[ID_S_OFF_ENETADDR + 3 * ID_BYTE_OFFSET],
		      p[ID_S_OFF_ENETADDR + 4 * ID_BYTE_OFFSET],
		      p[ID_S_OFF_ENETADDR + 5 * ID_BYTE_OFFSET]);
	      printf ("mfg date: ");
	      disp_prom_string (p + ID_S_OFF_MFGDATE, ID_S_MFGDATE_SIZE);
	      printf ("Oem flag: 0x%02x\n",
		      p[ID_S_OFF_OEM_FLAG]);
	      printf ("Banner: ");
	      disp_prom_string (p + ID_S_OFF_BANNER, ID_S_BANNER_SIZE);
	      printf ("\nCopyright: ");
	      disp_prom_string (p + ID_S_OFF_COPYRIGHT, ID_S_COPYRIGHT_SIZE);
	      printf ("\nLogo:\n");
	      for (j = 0; j < ID_S_LOGOBITMAP_SIZE; j++)
		{
		  int k;
		  c = p[ID_S_OFF_LOGOBITMAP + j * ID_BYTE_OFFSET];
		  for (k = 0; k < 8; k++)
		    {
		      putchar ((c & 0x80) ? '*': ' ');
		      c <<= 1;
		    }
		  if ((j & 7) == 7)
		    putchar ('\n');
		}
	      break;
	    case ID_GRAPHIC_BOARD:
	      {
		int nbr_res = p[ID_G_OFF_RES_COUNT];
		int k;

		printf ("Type: %d, nbr of plane: %d, access size: %d, "
			"nbr of resolutions: %d\n",
			p[ID_G_OFF_TYPE], p[ID_G_OFF_NBR_PLANE],
			p[ID_G_OFF_ACCESS_SIZE], nbr_res);
		p += ID_G_OFF_FB_SIZES;
		for (k = 0; k < nbr_res; k++)
		  {
		    printf ("Res %d: %dx%d, xoff: %d, "
			    "fb_size: 0x%08x, itbl: 0x%08x, font: 0x%08x\n",
			    k,
			    ID_2BYTES_TO_SHORT (p[ID_G_OFF_X_RES1],
						p[ID_G_OFF_X_RES2]),
			    ID_2BYTES_TO_SHORT (p[ID_G_OFF_Y_RES1],
						p[ID_G_OFF_Y_RES2]),
			    ID_2BYTES_TO_SHORT (p[ID_G_OFF_XOFFSET1],
						p[ID_G_OFF_XOFFSET2]),
			    ID_4BYTES_TO_LONG (p[ID_G_OFF_FB_SIZE1],
					       p[ID_G_OFF_FB_SIZE2],
					       p[ID_G_OFF_FB_SIZE3],
					       p[ID_G_OFF_FB_SIZE4]),
			    ID_4BYTES_TO_LONG (p[ID_G_OFF_ITBL_OFF1],
					       p[ID_G_OFF_ITBL_OFF2],
					       p[ID_G_OFF_ITBL_OFF3],
					       p[ID_G_OFF_ITBL_OFF4]),
			    ID_4BYTES_TO_LONG (p[ID_G_OFF_FONT_OFF1],
					       p[ID_G_OFF_FONT_OFF2],
					       p[ID_G_OFF_FONT_OFF3],
					       p[ID_G_OFF_FONT_OFF4]));
		    p += ID_G_SIZE_FB_SIZES;
		  }
		printf ("White value: 0x%08x, Black value: 0x%08x, "
			"fb offset: 0x%08x, kb offset: 0x%08x\n",
			ID_4BYTES_TO_LONG (p[ID_G_OFF_WHITE_OFF1],
					   p[ID_G_OFF_WHITE_OFF2],
					   p[ID_G_OFF_WHITE_OFF3],
					   p[ID_G_OFF_WHITE_OFF4]),
			ID_4BYTES_TO_LONG (p[ID_G_OFF_BLACK_OFF1],
					   p[ID_G_OFF_BLACK_OFF2],
					   p[ID_G_OFF_BLACK_OFF3],
					   p[ID_G_OFF_BLACK_OFF4]),
			ID_4BYTES_TO_LONG (p[ID_G_OFF_FB_OFF1],
					   p[ID_G_OFF_FB_OFF2],
					   p[ID_G_OFF_FB_OFF3],
					   p[ID_G_OFF_FB_OFF4]),
			ID_4BYTES_TO_LONG (p[ID_G_OFF_KB_OFF1],
					   p[ID_G_OFF_KB_OFF2],
					   p[ID_G_OFF_KB_OFF3],
					   p[ID_G_OFF_KB_OFF4]));
	      }
	      break;
	    }
	}
      if (offset != 0)
	{
	  printf ("Slot %d: ", i);
	  va += offset;
	  goto again;
	}
    }
}

void
disp_eeprom (void)
{
  unsigned char *va;
  unsigned char ch[16];
  int i, j;

  va = (unsigned char *) alloc_page ((daddr_t) 0x17002000, PG_IO | PG_V);

  printf ("Model: ");
  disp_prom_string (va + EEPROM_OFF_MODEL, EEPROM_SIZE_MODEL);
  printf ("\n");
  for (i = 0; i < EEPROM_SIZE; i += 16)
    {
      printf ("%03x: ", i);
      for (j = 0; j < 16; j++)
	{
	  ch[j] = va[(i + j) * EEPROM_BYTE_OFFSET];
	  printf ("%02x%c", ch[j], j == 7 ? '-' : ' ');
	}
      for (j = 0; j < 16; j++)
	putchar ((ch[j] < 32 || ch[j] > 127) ? '.' : ch[j]);
      putchar ('\n');
    }
}

#define ROM_VECTORS 0xff000000
void
disp_prom (void)
{
  unsigned long *addr;
  int i;
  struct prom_command_area *ca;
  
  printf ("version: %s\n", *(unsigned long *)ROM_VERSION);
  printf ("DGRAM version: 0x%08x\n", *(unsigned long *)ROM_DGRAM);
  printf ("EE version: 0x%08x\n", *(unsigned long *)ROM_EEVERSION);
  printf ("ROM version: 0x%08x\n", *(unsigned long *)ROM_REVISION);
  ca = *(struct prom_command_area **) ROM_COMM_AREA;
  printf ("first_free: 0x%08x\n", ca->first_free);
  printf ("memsize: %d Mb\n", ca->memsize);
  printf ("ramdisk: 0x%08x\n", ca->ramdisk);
  printf ("iomap_addr: 0x%08x\n", ca->iomap_addr);
  printf ("row: %d, col: %d\n", ca->row, ca->col);
  printf ("silent: %d\n", ca->silent);
  addr = (unsigned long *)ca->iomap_addr;
  for (i = 0; i < 10; i++)
    printf ("%d: 0x%08x\n", i, addr[i]);
  
}

void
start (int argc, char *argv[], char *envp[])
{
  int i;
  char *addr;
  char *pa;
  unsigned char c;

  set_led (0);
  bzero (&edata, &end - &edata);
  set_led (1);

  printf ("Hello world\n\r");
  fflush (stdout);

  /*  set_debug (); */
  setvbuf (stdout, NULL, _IONBF, 0);

  printf ("MMCR: 0x%08x\n", read_asi (ASI_MMCR, 0));
  printf ("PDBA: 0x%08x\n", read_asi (ASI_PDBA, 0));
  printf ("BID: 0x%08x\n", read_asi (ASI_BID, 0));

  printf ("ret: 0x%08x\n", debug_ret);
  printf ("tbr: old = 0x%08x, new = 0x%08x\n", debug_tbr, read_tbr ());
  
  printf ("Args:");
  for (i = 0; i < argc; i++)
    printf (" %s", argv[i]);
  putchar ('\n');
  for (i = 0; envp[i]; i++)
    printf ("%s\n", envp[i]);


/*  dump_dg (); */
  uninitialize_dg ();

#if 0
  disp (10);
  play_with_led ();
#endif

#if 0
  set_led (0);
  for (i = -1; i ; i--)
    ;
  set_led (1);
#endif
  disp_zs0_status ();
  nofault = 1;
  for (i = 0; i < 4; i++)
    {
      zs0_putc ('h');
      zs0_putc ('e');
      zs0_putc ('l');
    }
  zs0_putc ('\n');
  zs0_putc ('\r');
  if (nofault == 0)
    printf ("Fault\n");

  set_debug ();
  fflush (stdout);

  /* At first, initialize the mmu.  */
  init_mmu ();

  /* Then, the serial line.  */
  init_zs0 ();

  printf ("mmu Hello again\n");

  try_mmu (); 
  /* Probe the hardware.  */
  probe_kbus (); 
  /* disp_eeprom (); */
/*  disp_prom (); */

/*  try_dtlb (); */
/*  dump_dtlb (); */

  ta (127);
  _exit ();
#if 0
  dump_pde ((unsigned long*) 0xff23a000);
#endif
#if 0
  init_mmu ();
  try_mmu ();
#endif
  
  ta (127);

  initialize_dg ();
  write (1, "end\n", 4);
}

#define NUMREGS 80 

/* Number of bytes of registers.  */
#define NUMREGBYTES (NUMREGS * 4)
enum regnames
{
  G0, G1, G2, G3, G4, G5, G6, G7,
  O0, O1, O2, O3, O4, O5, SP, O7,
  L0, L1, L2, L3, L4, L5, L6, L7,
  I0, I1, I2, I3, I4, I5, FP, I7,
  
  F0, F1, F2, F3, F4, F5, F6, F7,
  F8, F9, F10, F11, F12, F13, F14, F15,
  F16, F17, F18, F19, F20, F21, F22, F23,
  F24, F25, F26, F27, F28, F29, F30, F31,
  Y, PSR, WIM, TBR, PC, NPC, FPSR, CPSR,
  MMCR, FCR, FVAR, PDBA, FPAR, FTOR, FES, FTSR}; /* MMCR at 72.  */


void
handle_exception (unsigned long *registers)
{
  int trap = (registers[TBR] >> 4) & 0xff;

  if (trap == 15)
    _exit ();

  if (1 || trap != 9 || !(nofault & NOFAULT_QUIET))
    {
      printf ("Trap %d:\n"
	      "psr = 0x%08x, tbr = 0x%08x, pc = 0x%08x, npc = 0x%08x\n",
	      trap,
	      registers[PSR], registers[TBR], registers[PC], registers[NPC]);
      printf ("sp = %08x, fp = %08x\n",
	      registers[SP], registers[FP]);
      printf ("MMCR = %08x, FCR = %08x, FVAR = %08x, PDBA = %08x\n",
	      registers[MMCR], registers[FCR],
	      registers[FVAR], registers[PDBA]);
      printf ("FPAR = %08x, FTOR = %08x, FES = %08x, FTSR = %08x\n",
	      registers[FPAR], registers[FTOR],
	      registers[FES], registers[FTSR]);
    }
  
  if (trap == 9 && (nofault & NOFAULT_CONTINUE))
    {
      nofault = 0;
      registers[PC] = registers[NPC];
      registers[NPC] += 4;
    }
  else
    _exit ();
}

