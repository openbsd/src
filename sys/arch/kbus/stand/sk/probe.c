#include <sys/types.h>
#include <machine/kbus.h>
#include "idprom.h"
#include "eeprom.h"
#include "prom.h"

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
