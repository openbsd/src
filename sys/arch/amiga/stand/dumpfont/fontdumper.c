/*	$NetBSD: fontdumper.c,v 1.3 1994/10/26 02:06:59 cgd Exp $	*/

/*
 * Routine to allow user to select from available fonts that fit restricitons of
 * NetBSD display code and then dump that font in the format for inclusion in the
 * kernel. Only character values 32-255 are dumped.
 *
 * Current kernel only allows fonts up to 8 pixels wide & non-proportional.
 * If this changes, the font requestor flags and restriction tests will need updating.
 * Also the NetBSDwidth value, cursor bits and dumping of font hex values needs updating.
 *
 * Author: Alan Bair
 * Dated:  11/12/1993
 *
 * Added printing of some other useful data for future (and current) expansion.
 * -ch 
 * Dated:  11/17/1993
 */

/* Original code by Markus Wild */
/* This is a *real* hack to dump the topaz80 kernel font. This one is 
   ways nicer than the ugly Mach font, but we'll have to dump it from a
   running system to not run against Commodore copyrights. *NEVER* distribute
   the generated font with BSD, always regenerate! */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <libraries/asl.h>

#include <inline/exec.h>
#include <inline/graphics.h>

#include <stdio.h>

#define NetBSDwidth	8

main(int argc, char *argv[])
{
    unsigned char str[256];
    int i;
    int j;
    struct RastPort rp;
    unsigned char *pp;
    struct BitMap bm = {
	256, 	/* bytes per row */
	8,	/* rows */
	0,	/* flags */
	1,	/* depth */
	0,	/* pad */
	0 	/* planes */
	};
    struct TextAttr ta;
    struct TextFont *tf;
    struct FontRequester *fr;
    struct TagItem frtags[] = {
	ASL_Hail, (ULONG)"NetBSD font choices",
	ASL_Width, 640,
	ASL_Height, 400,
	ASL_LeftEdge, 10,
	ASL_TopEdge, 10,
	ASL_OKText, (ULONG)"Dump",
	ASL_CancelText, (ULONG)"Cancel",
	ASL_FontName, (ULONG)"topaz.font",
	ASL_FontHeight, 8L,
	ASL_FontStyles, FS_NORMAL,
	ASL_FuncFlags, FONF_STYLES | FONF_FIXEDWIDTH,
	TAG_DONE
	    };

    /* Let the user pick a font to dump */
    if (fr = (struct FontRequester *)
	AllocAslRequest(ASL_FontRequest, frtags)) {
	if (!AslRequest(fr, NULL)) {
	    FreeAslRequest(fr);
	    fprintf(stderr, "User requested exit\n");
	    exit (0);
	}
	ta.ta_Name = (STRPTR)malloc(strlen(fr->fo_Attr.ta_Name));
	strcpy(ta.ta_Name, fr->fo_Attr.ta_Name);
	ta.ta_YSize = fr->fo_Attr.ta_YSize;
	ta.ta_Style = fr->fo_Attr.ta_Style;
	ta.ta_Flags = fr->fo_Attr.ta_Flags;
	FreeAslRequest(fr);
    } else {
	fprintf(stderr, "Can't allocate Font Requestor\n");
	exit (1);
    }

    /* Open the selected font */
    tf = (struct TextFont *)OpenDiskFont (&ta);
    if (! tf) {
	fprintf (stderr, "Can't open font: %s\n", ta.ta_Name);
	exit (1);
    }
#ifdef DEBUG
    fprintf(stderr, "Information on selected font:\n");
    fprintf(stderr, "Name=%s\n", ta.ta_Name);
    fprintf(stderr, "Height=%d tf_Style=%x tf_Flags=%x Width=%d Baseline=%d\n",
	    tf->tf_YSize, tf->tf_Style, tf->tf_Flags, tf->tf_XSize, tf->tf_Baseline);
#endif

    /* Check for NetBSD restrictions */
    if (tf->tf_Flags & FPF_PROPORTIONAL) {
	fprintf(stderr, "NetBSD does not support proportional fonts\n");
	exit (1);
    }
    if (tf->tf_XSize > NetBSDwidth) {
	fprintf(stderr, "NetBSD does not support fonts wider than %d pixels\n", NetBSDwidth);
	exit (1);
    }

    /* Allocate area to render font in */
    InitBitMap(&bm, 1, 256 * NetBSDwidth, tf->tf_YSize);
    InitRastPort (&rp);
    rp.BitMap = &bm;
    bm.Planes[0] = pp = AllocRaster (256 * NetBSDwidth, tf->tf_YSize);
    if (!pp) {
	fprintf (stderr, "Can't allocate raster!\n");
	exit (1);
    }

    /* Initialize string to be rendered */
    for (i = 32; i < 256; i++) {
	str[i - 32] = i;
    }

    /* Render string with selected font */
    SetFont (&rp, tf);
    SetSoftStyle(&rp, ta.ta_Style ^ tf->tf_Style, (FSF_BOLD | FSF_UNDERLINED | FSF_ITALIC));
    Move (&rp, 0, tf->tf_Baseline);
    ClearEOL(&rp);
    if (tf->tf_XSize != NetBSDwidth) {
	/* right-justify char in cell */
	Move (&rp, NetBSDwidth - tf->tf_XSize, tf->tf_Baseline);
	/* Narrow font, put each character in space of normal font */
	for (i = 0; i < (256 - 32); i++) {
	    Text (&rp, &str[i], 1);
	    Move (&rp, rp.cp_x + (NetBSDwidth - tf->tf_XSize), rp.cp_y);
	}
    } else {
	Text (&rp, str, 256 - 32);
    }
  
    /* Dump them.. */
    printf ("/* Generated automatically by fontdumper.c. *DONT* distribute\n");
    printf ("   this file, it may contain information Copyright by Commodore!\n");
    printf ("\n");
    printf ("   Font: %s/%d\n", ta.ta_Name, tf->tf_YSize);
    printf (" */\n\n");
    
    printf ("unsigned char kernel_font_width  = %d;\n", tf->tf_XSize);
    printf ("unsigned char kernel_font_height = %d;\n", tf->tf_YSize);
    printf ("unsigned char kernel_font_baseline = %d;\n", tf->tf_Baseline);
    printf ("short         kernel_font_boldsmear = %d;\n", tf->tf_BoldSmear);
    printf ("unsigned char kernel_font_lo = 32;\n");
    printf ("unsigned char kernel_font_hi = 255;\n\n");
    
    printf ("unsigned char kernel_cursor[] = {\n");
    for (j = 0; j < (tf->tf_YSize -1); j++) {
	printf ("0xff, ");
    }
    printf ("0xff };\n\n");

    printf ("unsigned char kernel_font[] = {\n");
    for (i = 0; i < 256 - 32; i++) {
	printf ("/* %c */", i + 32);
	for (j = 0; j < tf->tf_YSize; j++) {
	    printf (" 0x%02x,", pp[i+j*256]);
	    }
	printf ("\n");
    }
    printf ("};\n");
    
    CloseFont (tf);
    FreeRaster (pp, 256 * NetBSDwidth, tf->tf_YSize);
    return (0);
}
