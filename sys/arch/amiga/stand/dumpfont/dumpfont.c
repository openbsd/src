/*	$NetBSD: dumpfont.c,v 1.5 1994/10/26 02:06:57 cgd Exp $	*/

/*
 * This is a *real* hack to dump the topaz80 kernel font. This one is 
 * ways nicer than the ugly Mach font, but we'll have to dump it from a
 * running system to not run against Commodore copyrights. *NEVER* distribute
 * the generated font with BSD, always regenerate! 
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/text.h>

#include <inline/exec.h>
#include <inline/graphics.h>

#include <stdio.h>


main()
{
  unsigned char str[256], *pp;
  int i;
  struct TextAttr ta = { "topaz.font", 8, FS_NORMAL, FPF_ROMFONT };
  struct RastPort rp;
  struct BitMap bm = { 256, 	/* bytes per row */
		         8,	/* rows */
			 0,	/* flags */
			 1,	/* depth */
			 0,	/* pad */
			 0 };	/* planes */
  struct TextFont *tf;

  InitRastPort (& rp);
  rp.BitMap = &bm;
  bm.Planes[0] = pp = AllocRaster (256 * 8, 8);

  if (!pp)
    {
      fprintf (stderr, "Can't allocate raster!\n");
      exit (1);
    }
  bzero (pp, 256 * 8);
  
  tf = OpenFont (& ta);
  if (! tf)
    {
      fprintf (stderr, "can't open topaz font.\n");
      exit (1);
    }

  SetFont (&rp, tf);

  /* initialize string to be printed */
  for (i = 32; i < 256; i++) str[i - 32] = i;

  Move (&rp, 0, 6);
  
  Text (&rp, str, 256 - 32);
  {
    int bin = open ("bitmap", 1);
    if (bin >= 0)
      {
        write (bin, pp, 256*8);
        close (bin);
      }
  }
  
  /* dump them.. */
  printf ("/* generated automatically by dumpfont.c. *DONT* distribute\n");
  printf ("   this file, it contains information Copyright by Commodore!\n");
  printf ("\n");
  printf ("   This is the (new) topaz80 system font: */\n\n");
  
  printf ("unsigned char kernel_font_width  = 8;\n");
  printf ("unsigned char kernel_font_height = 8;\n");
  printf ("unsigned char kernel_font_lo = 32;\n");
  printf ("unsigned char kernel_font_hi = 255;\n\n");
  
  printf ("unsigned char kernel_cursor[] = {\n");
  printf ("  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };\n\n");
  printf ("unsigned char kernel_font[] = {\n");
  
  for (i = 0; i < 256 - 32; i++)
    {
      printf ("/* %c */ ", i + 32);
      printf ("0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x,\n",
      	      pp[i+0*256], pp[i+1*256], pp[i+2*256], pp[i+3*256],
      	      pp[i+4*256], pp[i+5*256], pp[i+6*256], pp[i+7*256]);
    }
  printf ("};\n");
  
  CloseFont (tf);
  FreeRaster (pp, 256 * 8, 8);
}
