/*
 *  UCdomap.c
 *  =========
 *
 * This is a Lynx chartrans engine, its external calls are in UCMap.h
 *
 * Derived from code in the Linux kernel console driver.
 * The GNU Public Licence therefore applies, see
 * the file COPYING in the top-level directory
 * which should come with every Lynx distribution.
 *
 *  [ original comment: - KW ]
 *
 * Mapping from internal code (such as Latin-1 or Unicode or IBM PC code)
 * to font positions.
 *
 * aeb, 950210
 */
#include <HTUtils.h>
#include <HTMLDTD.h>

#include <LYGlobalDefs.h>
#include <UCkd.h>
#include <UCdomap.h>
#include <UCMap.h>
#include <UCDefs.h>
#include <LYCharSets.h>

#include <LYLeaks.h>

/*
 *  Include chartrans tables:
 */
#include <cp1250_uni.h> 	/* WinLatin2 (cp1250)	*/
#include <cp1251_uni.h> 	/* WinCyrillic (cp1251) */
#include <cp1252_uni.h> 	/* WinLatin1 (cp1252)	*/
#include <cp1253_uni.h> 	/* WinGreek (cp1253)	*/
#include <cp1255_uni.h> 	/* WinHebrew (cp1255)	*/
#include <cp1256_uni.h> 	/* WinArabic (cp1256)	*/
#include <cp1257_uni.h> 	/* WinBaltRim (cp1257)	*/
#include <cp437_uni.h>		/* DosLatinUS (cp437)	*/
#include <cp737_uni.h>		/* DosGreek (cp737)	*/
#include <cp775_uni.h>		/* DosBaltRim (cp775)	*/
#include <cp850_uni.h>		/* DosLatin1 (cp850)	*/
#include <cp852_uni.h>		/* DosLatin2 (cp852)	*/
#include <cp862_uni.h>		/* DosHebrew (cp862)	*/
#include <cp864_uni.h>		/* DosArabic (cp864)	*/
#include <cp866_uni.h>		/* DosCyrillic (cp866)	*/
#include <cp869_uni.h>		/* DosGreek2 (cp869)	*/
#include <def7_uni.h>		/* 7 bit approximations */
#include <dmcs_uni.h>		/* DEC Multinational	*/
#include <hp_uni.h>		/* HP Roman8		*/
#include <iso01_uni.h>		/* ISO Latin 1		*/
#include <iso02_uni.h>		/* ISO Latin 2		*/
#include <iso03_uni.h>		/* ISO Latin 3		*/
#include <iso04_uni.h>		/* ISO Latin 4		*/
#include <iso05_uni.h>		/* ISO 8859-5 Cyrillic	*/
#include <iso06_uni.h>		/* ISO 8859-6 Arabic	*/
#include <iso07_uni.h>		/* ISO 8859-7 Greek	*/
#include <iso08_uni.h>		/* ISO 8859-8 Hebrew	*/
#include <iso09_uni.h>		/* ISO 8859-9 (Latin 5) */
#include <iso10_uni.h>		/* ISO 8859-10		*/
#include <iso15_uni.h>		/* ISO 8859-15 (Latin 9)*/
#include <koi8r_uni.h>		/* KOI8-R Cyrillic	*/
#include <mac_uni.h>		/* Macintosh (8 bit)	*/
#include <mnem2_suni.h> 	/* RFC 1345 Mnemonic	*/
#include <next_uni.h>		/* NeXT character set	*/
#include <rfc_suni.h>		/* RFC 1345 w/o Intro	*/
/* #include <utf8_uni.h> */     /* UNICODE UTF 8        */
#include <viscii_uni.h> 	/* Vietnamese (VISCII)	*/
#include <cp866u_uni.h>		/* Ukrainian Cyrillic (866) */
#include <koi8u_uni.h>		/* Ukrainian Cyrillic (koi8-u */
#ifdef NOTDEFINED
#include <mnem_suni.h>
#endif /* NOTDEFINED */

/*
 *  Some of the code below, and some of the comments, are left in for
 *  historical reasons.  Not all those tables below are currently
 *  really needed (and what with all those hardwired codepoints),
 *  but let's keep them around for now.  They may come in handy if we
 *  decide to make more extended use of the mechanisms (including e.g.
 *  for chars < 127...).  - KW
 */

PRIVATE u16 translations[][256] = {
  /*
   *  8-bit Latin-1 mapped to Unicode -- trivial mapping.
   */
  {
    0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
    0x0008, 0x0009, 0x000a, 0x000b, 0x000c, 0x000d, 0x000e, 0x000f,
    0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017,
    0x0018, 0x0019, 0x001a, 0x001b, 0x001c, 0x001d, 0x001e, 0x001f,
    0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
    0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,
    0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
    0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
    0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
    0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f,
    0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
    0x0058, 0x0059, 0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f,
    0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
    0x0068, 0x0069, 0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f,
    0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
    0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d, 0x007e, 0x007f,
    0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085, 0x0086, 0x0087,
    0x0088, 0x0089, 0x008a, 0x008b, 0x008c, 0x008d, 0x008e, 0x008f,
    0x0090, 0x0091, 0x0092, 0x0093, 0x0094, 0x0095, 0x0096, 0x0097,
    0x0098, 0x0099, 0x009a, 0x009b, 0x009c, 0x009d, 0x009e, 0x009f,
    0x00a0, 0x00a1, 0x00a2, 0x00a3, 0x00a4, 0x00a5, 0x00a6, 0x00a7,
    0x00a8, 0x00a9, 0x00aa, 0x00ab, 0x00ac, 0x00ad, 0x00ae, 0x00af,
    0x00b0, 0x00b1, 0x00b2, 0x00b3, 0x00b4, 0x00b5, 0x00b6, 0x00b7,
    0x00b8, 0x00b9, 0x00ba, 0x00bb, 0x00bc, 0x00bd, 0x00be, 0x00bf,
    0x00c0, 0x00c1, 0x00c2, 0x00c3, 0x00c4, 0x00c5, 0x00c6, 0x00c7,
    0x00c8, 0x00c9, 0x00ca, 0x00cb, 0x00cc, 0x00cd, 0x00ce, 0x00cf,
    0x00d0, 0x00d1, 0x00d2, 0x00d3, 0x00d4, 0x00d5, 0x00d6, 0x00d7,
    0x00d8, 0x00d9, 0x00da, 0x00db, 0x00dc, 0x00dd, 0x00de, 0x00df,
    0x00e0, 0x00e1, 0x00e2, 0x00e3, 0x00e4, 0x00e5, 0x00e6, 0x00e7,
    0x00e8, 0x00e9, 0x00ea, 0x00eb, 0x00ec, 0x00ed, 0x00ee, 0x00ef,
    0x00f0, 0x00f1, 0x00f2, 0x00f3, 0x00f4, 0x00f5, 0x00f6, 0x00f7,
    0x00f8, 0x00f9, 0x00fa, 0x00fb, 0x00fc, 0x00fd, 0x00fe, 0x00ff
  },
  /*
   *  VT100 graphics mapped to Unicode.
   */
  {
    0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
    0x0008, 0x0009, 0x000a, 0x000b, 0x000c, 0x000d, 0x000e, 0x000f,
    0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017,
    0x0018, 0x0019, 0x001a, 0x001b, 0x001c, 0x001d, 0x001e, 0x001f,
    0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
    0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,
    0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
    0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
    0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
    0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f,
    0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
    0x0058, 0x0059, 0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x00a0,
    0x25c6, 0x2592, 0x2409, 0x240c, 0x240d, 0x240a, 0x00b0, 0x00b1,
    0x2424, 0x240b, 0x2518, 0x2510, 0x250c, 0x2514, 0x253c, 0xf800,
    0xf801, 0x2500, 0xf803, 0xf804, 0x251c, 0x2524, 0x2534, 0x252c,
    0x2502, 0x2264, 0x2265, 0x03c0, 0x2260, 0x00a3, 0x00b7, 0x007f,
    0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085, 0x0086, 0x0087,
    0x0088, 0x0089, 0x008a, 0x008b, 0x008c, 0x008d, 0x008e, 0x008f,
    0x0090, 0x0091, 0x0092, 0x0093, 0x0094, 0x0095, 0x0096, 0x0097,
    0x0098, 0x0099, 0x009a, 0x009b, 0x009c, 0x009d, 0x009e, 0x009f,
    0x00a0, 0x00a1, 0x00a2, 0x00a3, 0x00a4, 0x00a5, 0x00a6, 0x00a7,
    0x00a8, 0x00a9, 0x00aa, 0x00ab, 0x00ac, 0x00ad, 0x00ae, 0x00af,
    0x00b0, 0x00b1, 0x00b2, 0x00b3, 0x00b4, 0x00b5, 0x00b6, 0x00b7,
    0x00b8, 0x00b9, 0x00ba, 0x00bb, 0x00bc, 0x00bd, 0x00be, 0x00bf,
    0x00c0, 0x00c1, 0x00c2, 0x00c3, 0x00c4, 0x00c5, 0x00c6, 0x00c7,
    0x00c8, 0x00c9, 0x00ca, 0x00cb, 0x00cc, 0x00cd, 0x00ce, 0x00cf,
    0x00d0, 0x00d1, 0x00d2, 0x00d3, 0x00d4, 0x00d5, 0x00d6, 0x00d7,
    0x00d8, 0x00d9, 0x00da, 0x00db, 0x00dc, 0x00dd, 0x00de, 0x00df,
    0x00e0, 0x00e1, 0x00e2, 0x00e3, 0x00e4, 0x00e5, 0x00e6, 0x00e7,
    0x00e8, 0x00e9, 0x00ea, 0x00eb, 0x00ec, 0x00ed, 0x00ee, 0x00ef,
    0x00f0, 0x00f1, 0x00f2, 0x00f3, 0x00f4, 0x00f5, 0x00f6, 0x00f7,
    0x00f8, 0x00f9, 0x00fa, 0x00fb, 0x00fc, 0x00fd, 0x00fe, 0x00ff
  },
  /*
   *  IBM Codepage 437 mapped to Unicode.
   */
  {
    0x0000, 0x263a, 0x263b, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022,
    0x25d8, 0x25cb, 0x25d9, 0x2642, 0x2640, 0x266a, 0x266b, 0x263c,
    0x25ba, 0x25c4, 0x2195, 0x203c, 0x00b6, 0x00a7, 0x25ac, 0x21a8,
    0x2191, 0x2193, 0x2192, 0x2190, 0x221f, 0x2194, 0x25b2, 0x25bc,
    0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
    0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,
    0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
    0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
    0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
    0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f,
    0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
    0x0058, 0x0059, 0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f,
    0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
    0x0068, 0x0069, 0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f,
    0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
    0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d, 0x007e, 0x2302,
    0x00c7, 0x00fc, 0x00e9, 0x00e2, 0x00e4, 0x00e0, 0x00e5, 0x00e7,
    0x00ea, 0x00eb, 0x00e8, 0x00ef, 0x00ee, 0x00ec, 0x00c4, 0x00c5,
    0x00c9, 0x00e6, 0x00c6, 0x00f4, 0x00f6, 0x00f2, 0x00fb, 0x00f9,
    0x00ff, 0x00d6, 0x00dc, 0x00a2, 0x00a3, 0x00a5, 0x20a7, 0x0192,
    0x00e1, 0x00ed, 0x00f3, 0x00fa, 0x00f1, 0x00d1, 0x00aa, 0x00ba,
    0x00bf, 0x2310, 0x00ac, 0x00bd, 0x00bc, 0x00a1, 0x00ab, 0x00bb,
    0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
    0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 0x255c, 0x255b, 0x2510,
    0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 0x255e, 0x255f,
    0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x2567,
    0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b,
    0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580,
    0x03b1, 0x00df, 0x0393, 0x03c0, 0x03a3, 0x03c3, 0x00b5, 0x03c4,
    0x03a6, 0x0398, 0x03a9, 0x03b4, 0x221e, 0x03c6, 0x03b5, 0x2229,
    0x2261, 0x00b1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00f7, 0x2248,
    0x00b0, 0x2219, 0x00b7, 0x221a, 0x207f, 0x00b2, 0x25a0, 0x00a0
  },
  /*
   *  User mapping -- default to codes for direct font mapping.
   */
  {
    0xf000, 0xf001, 0xf002, 0xf003, 0xf004, 0xf005, 0xf006, 0xf007,
    0xf008, 0xf009, 0xf00a, 0xf00b, 0xf00c, 0xf00d, 0xf00e, 0xf00f,
    0xf010, 0xf011, 0xf012, 0xf013, 0xf014, 0xf015, 0xf016, 0xf017,
    0xf018, 0xf019, 0xf01a, 0xf01b, 0xf01c, 0xf01d, 0xf01e, 0xf01f,
    0xf020, 0xf021, 0xf022, 0xf023, 0xf024, 0xf025, 0xf026, 0xf027,
    0xf028, 0xf029, 0xf02a, 0xf02b, 0xf02c, 0xf02d, 0xf02e, 0xf02f,
    0xf030, 0xf031, 0xf032, 0xf033, 0xf034, 0xf035, 0xf036, 0xf037,
    0xf038, 0xf039, 0xf03a, 0xf03b, 0xf03c, 0xf03d, 0xf03e, 0xf03f,
    0xf040, 0xf041, 0xf042, 0xf043, 0xf044, 0xf045, 0xf046, 0xf047,
    0xf048, 0xf049, 0xf04a, 0xf04b, 0xf04c, 0xf04d, 0xf04e, 0xf04f,
    0xf050, 0xf051, 0xf052, 0xf053, 0xf054, 0xf055, 0xf056, 0xf057,
    0xf058, 0xf059, 0xf05a, 0xf05b, 0xf05c, 0xf05d, 0xf05e, 0xf05f,
    0xf060, 0xf061, 0xf062, 0xf063, 0xf064, 0xf065, 0xf066, 0xf067,
    0xf068, 0xf069, 0xf06a, 0xf06b, 0xf06c, 0xf06d, 0xf06e, 0xf06f,
    0xf070, 0xf071, 0xf072, 0xf073, 0xf074, 0xf075, 0xf076, 0xf077,
    0xf078, 0xf079, 0xf07a, 0xf07b, 0xf07c, 0xf07d, 0xf07e, 0xf07f,
    0xf080, 0xf081, 0xf082, 0xf083, 0xf084, 0xf085, 0xf086, 0xf087,
    0xf088, 0xf089, 0xf08a, 0xf08b, 0xf08c, 0xf08d, 0xf08e, 0xf08f,
    0xf090, 0xf091, 0xf092, 0xf093, 0xf094, 0xf095, 0xf096, 0xf097,
    0xf098, 0xf099, 0xf09a, 0xf09b, 0xf09c, 0xf09d, 0xf09e, 0xf09f,
    0xf0a0, 0xf0a1, 0xf0a2, 0xf0a3, 0xf0a4, 0xf0a5, 0xf0a6, 0xf0a7,
    0xf0a8, 0xf0a9, 0xf0aa, 0xf0ab, 0xf0ac, 0xf0ad, 0xf0ae, 0xf0af,
    0xf0b0, 0xf0b1, 0xf0b2, 0xf0b3, 0xf0b4, 0xf0b5, 0xf0b6, 0xf0b7,
    0xf0b8, 0xf0b9, 0xf0ba, 0xf0bb, 0xf0bc, 0xf0bd, 0xf0be, 0xf0bf,
    0xf0c0, 0xf0c1, 0xf0c2, 0xf0c3, 0xf0c4, 0xf0c5, 0xf0c6, 0xf0c7,
    0xf0c8, 0xf0c9, 0xf0ca, 0xf0cb, 0xf0cc, 0xf0cd, 0xf0ce, 0xf0cf,
    0xf0d0, 0xf0d1, 0xf0d2, 0xf0d3, 0xf0d4, 0xf0d5, 0xf0d6, 0xf0d7,
    0xf0d8, 0xf0d9, 0xf0da, 0xf0db, 0xf0dc, 0xf0dd, 0xf0de, 0xf0df,
    0xf0e0, 0xf0e1, 0xf0e2, 0xf0e3, 0xf0e4, 0xf0e5, 0xf0e6, 0xf0e7,
    0xf0e8, 0xf0e9, 0xf0ea, 0xf0eb, 0xf0ec, 0xf0ed, 0xf0ee, 0xf0ef,
    0xf0f0, 0xf0f1, 0xf0f2, 0xf0f3, 0xf0f4, 0xf0f5, 0xf0f6, 0xf0f7,
    0xf0f8, 0xf0f9, 0xf0fa, 0xf0fb, 0xf0fc, 0xf0fd, 0xf0fe, 0xf0ff
  }
};
PRIVATE u16 *UC_translate = NULL;

PRIVATE struct UC_charset UCInfo[MAXCHARSETS];

/*
 *  The standard kernel character-to-font mappings are not invertible
 *  -- this is just a best effort.
 */
#define MAX_GLYPH 512		/* Max possible glyph value */

PRIVATE unsigned char * inv_translate = NULL;
PRIVATE unsigned char inv_norm_transl[MAX_GLYPH];
PRIVATE unsigned char * inverse_translations[4] = { NULL, NULL, NULL, NULL };

PRIVATE void set_inverse_transl PARAMS((
	int		i));
PRIVATE u16 *set_translate PARAMS((
	int		m));
#ifdef NOTDEFINED
PRIVATE unsigned char inverse_translate PARAMS((int glyph));
PRIVATE int con_set_trans_old PARAMS((unsigned char *arg));
PRIVATE int con_get_trans_old PARAMS((unsigned char *arg));
PRIVATE int con_set_trans_new PARAMS((u16 *arg));
PRIVATE int con_get_trans_new PARAMS((u16 *arg));
#endif /* NOTDEFINED */
PRIVATE int UC_valid_UC_charset PARAMS((
	int		UC_charset_hndl));
PRIVATE void UC_con_set_trans PARAMS((
	int		UC_charset_in_hndl,
	int		Gn,
	int		update_flag));
PRIVATE int con_insert_unipair PARAMS((
	u16		unicode,
	u16		fontpos,
	int		fordefault));
PRIVATE int con_insert_unipair_str PARAMS((
	u16		unicode,
	CONST char *	replace_str,
	int		fordefault));
PRIVATE void con_clear_unimap PARAMS((
	int		fordefault));
PRIVATE void con_clear_unimap_str PARAMS((
	int		fordefault));
#ifdef NOTDEFINED
PRIVATE int con_set_unimap PARAMS((
	u16			ct,
	struct unipair *	list));
#endif /* NOTDEFINED */
PRIVATE void con_set_default_unimap NOPARAMS;
PRIVATE int UC_con_set_unimap PARAMS((
	int		UC_charset_out_hndl,
	int		update_flag));
PRIVATE int UC_con_set_unimap_str PARAMS((
	u16			ct,
	struct unipair_str *	list,
	int			fordefault));
#ifdef NOTDEFINED
PRIVATE int con_get_unimap PARAMS((
	u16			ct,
	u16 *			uct,
	struct unipair *	list));
#endif /* NOTDEFINED */
PRIVATE int conv_uni_to_pc PARAMS((
	long			ucs,
	int			usedefault));
PRIVATE int conv_uni_to_str PARAMS((
	char*		outbuf,
	int		buflen,
	long		ucs,
	int		usedefault));
PRIVATE void UCconsole_map_init NOPARAMS;
PRIVATE int UC_MapGN PARAMS((
	int		UChndl,
	int		update_flag));
PRIVATE int UC_FindGN_byMIME PARAMS((
	CONST char *	UC_MIMEcharset));
PRIVATE void UCreset_allocated_LYCharSets NOPARAMS;
PRIVATE CONST char ** UC_setup_LYCharSets_repl PARAMS((
	int		UC_charset_in_hndl,
	unsigned	lowest8));
PRIVATE int UC_Register_with_LYCharSets PARAMS((
	int		s,
	CONST char *	UC_MIMEcharset,
	CONST char *	UC_LYNXcharset,
	int		lowest_eightbit));
#ifdef LY_FIND_LEAKS
PRIVATE void UCfree_allocated_LYCharSets NOPARAMS;
PRIVATE void UCcleanup_mem NOPARAMS;
#endif

PRIVATE int default_UChndl = -1;

PRIVATE void set_inverse_transl ARGS1(
	int,		i)
{
    int j, glyph;
    u16 *p = translations[i];
    unsigned char *q = inverse_translations[i];

    if (!q) {
	/*
	 *  Slightly messy to avoid calling kmalloc too early.
	 */
	q = inverse_translations[i] = ((i == LAT1_MAP) ?
				       inv_norm_transl :
				       (unsigned char *)malloc(MAX_GLYPH));
	if (!q)
	    return;
    }
    for (j = 0; j < MAX_GLYPH; j++)
	q[j] = 0;

    for (j = 0; j < E_TABSZ; j++) {
	glyph = conv_uni_to_pc(p[j], 0);
	if (glyph >= 0 && glyph < MAX_GLYPH && q[glyph] < 32) {
	    /*
	     *	Prefer '-' above SHY etc.
	     */
	    q[glyph] = j;
	}
    }
}

PRIVATE u16 *set_translate ARGS1(
	int,		m)
{
	if (!inverse_translations[m])
		set_inverse_transl(m);
	inv_translate = inverse_translations[m];
	return translations[m];
}

#ifdef NOTDEFINED
/*
 * Inverse translation is impossible for several reasons:
 * 1. The font<->character maps are not 1-1.
 * 2. The text may have been written while a different translation map
 *    was active, or using Unicode.
 * Still, it is now possible to a certain extent to cut and paste non-ASCII.
 */
PRIVATE unsigned char inverse_translate ARGS1(
	int,		glyph)
{
    if (glyph < 0 || glyph >= MAX_GLYPH) {
		return 0;
    } else {
	return ((inv_translate && inv_translate[glyph]) ?
				   inv_translate[glyph] :
				   (unsigned char)(glyph & 0xff));
    }
}

/*
 *  Load customizable translation table.
 *  'arg' points to a 256 byte translation table.
 *
 *  The "old" variants are for translation directly to font (using the
 *  0xf000-0xf0ff "transparent" Unicodes) whereas the "new" variants set
 *  Unicodes explicitly.
 */
PRIVATE int con_set_trans_old ARGS1(
	unsigned char *,	arg)
{
    int i;
    u16 *p = translations[USER_MAP];
#if(0)
    i = verify_area(VERIFY_READ, (void *)arg, E_TABSZ);
    if (i)
	return i;
#endif
    for (i = 0; i < E_TABSZ; i++)
	p[i] = UNI_DIRECT_BASE | (u16)arg[i];

    set_inverse_transl(USER_MAP);
    return 0;
}

PRIVATE int con_get_trans_old ARGS1(
	unsigned char *,	arg)
{
    int i, ch;
    u16 *p = translations[USER_MAP];
#if(0)
    i = verify_area(VERIFY_WRITE, (void *)arg, E_TABSZ);
    if (i)
	return i;
#endif
    for (i = 0; i < E_TABSZ; i++) {
	ch = conv_uni_to_pc(p[i]);
#ifdef NOTDEFINED
	put_user((ch & ~0xff) ? 0 : ch, arg+i);
#endif /* NOTDEFINED */
	arg[i] = (unsigned char)((ch & ~0xff) ? 0 : ch);
    }
    return 0;
}

PRIVATE int con_set_trans_new ARGS1(
	u16 *,		arg)
{
    int i;
    u16 *p = translations[USER_MAP];
#if(0)
    i = verify_area(VERIFY_READ, (void *)arg, E_TABSZ*sizeof(u16));
    if (i)
	return i;
#endif
    for (i = 0; i < E_TABSZ; i++)
	p[i] = arg[i];

    set_inverse_transl(USER_MAP);
    return 0;
}

PRIVATE int con_get_trans_new ARGS1(
	u16 *		arg)
{
    int i;
    u16 *p = translations[USER_MAP];
#if(0)
    i = verify_area(VERIFY_WRITE, (void *)arg, E_TABSZ*sizeof(u16));
    if (i)
	return i;
#endif
    for (i = 0; i < E_TABSZ; i++)
	arg[i] = p[i];

    return 0;
}
#endif /* NOTDEFINED */

PRIVATE int UC_valid_UC_charset ARGS1(
	int,		UC_charset_hndl)
{
  return (UC_charset_hndl >= 0 && UC_charset_hndl < UCNumCharsets);
}

PRIVATE void UC_con_set_trans ARGS3(
	int,		UC_charset_in_hndl,
	int,		Gn,
	int,		update_flag)
{
    int i, j;
    CONST u16 *p;
    u16 *ptrans;

    if (!UC_valid_UC_charset(UC_charset_in_hndl)) {
	CTRACE(tfp, "UC_con_set_trans: Invalid charset handle %d.\n",
		    UC_charset_in_hndl);
	return;
    }
    ptrans = translations[Gn];
    p = UCInfo[UC_charset_in_hndl].unitable;
#if(0)
    if (p == UC_current_unitable) {    /* test whether pointers are equal */
	return;			/* nothing to be done */
    }
    /*
     *	The font is always 256 characters - so far.
     *  (this function preserved by num_uni==0 so unicount=NULL for built-in
     *  charsets like CJK or x-transparent should not be a problem?)
     */
    con_clear_unimap();
#endif
    for (i = 0; i < 256; i++) {
	if ((j = UCInfo[UC_charset_in_hndl].unicount[i])) {
	    ptrans[i] = *p;
	    for (; j; j--) {
		p++;
	    }
	} else {
	    ptrans[i] = 0xfffd;
	}
    }
    if (update_flag) {
	set_inverse_transl(Gn); /* Update inverse translation for this one */
    }
}

/*
 * Unicode -> current font conversion
 *
 * A font has at most 512 chars, usually 256.
 * But one font position may represent several Unicode chars.
 * A hashtable is somewhat of a pain to deal with, so use a
 * "paged table" instead.  Simulation has shown the memory cost of
 * this 3-level paged table scheme to be comparable to a hash table.
 */
PRIVATE int hashtable_contents_valid = 0; /* Use ASCII-only mode for bootup*/
PRIVATE int hashtable_str_contents_valid = 0;

PRIVATE u16 **uni_pagedir[32] =
{
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

PRIVATE char* **uni_pagedir_str[32] =
{
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

PRIVATE CONST u16 * UC_current_unitable = NULL;
PRIVATE struct unimapdesc_str *UC_current_unitable_str = NULL;

/*
 *  Keep a second set of structures for the translation designated
 *  as "default" - kw
 */
PRIVATE int unidefault_contents_valid = 0; /* Use ASCII-only mode for bootup*/
PRIVATE int unidefault_str_contents_valid = 0;

static u16 **unidefault_pagedir[32] =
{
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};
static char* **unidefault_pagedir_str[32] =
{
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

PRIVATE CONST u16 * UC_default_unitable = 0;
PRIVATE CONST struct unimapdesc_str *UC_default_unitable_str = 0;

PRIVATE int con_insert_unipair ARGS3(
	u16,		unicode,
	u16,		fontpos,
	int,		fordefault)
{
    int i, n;
    u16 **p1, *p2;

    if(fordefault)
	p1 = unidefault_pagedir[n = unicode >> 11];
    else
	p1 = uni_pagedir[n = unicode >> 11];
    if (!p1) {
	p1 = (u16* *)malloc(32*sizeof(u16 *));
	if (fordefault)
	    unidefault_pagedir[n] = p1;
	else
	    uni_pagedir[n] = p1;
	if ( !p1 )
	    return -ENOMEM;

	for (i = 0; i < 32; i++) {
	    p1[i] = NULL;
	}
    }

    if (!(p2 = p1[n = (unicode >> 6) & 0x1f])) {
	p2 = p1[n] = (u16 *)malloc(64*sizeof(u16));
	if (!p2)
	    return -ENOMEM;

	for (i = 0; i < 64; i++) {
	    p2[i] = 0xffff;		/* No glyph for this character (yet) */
	}
    }

    p2[unicode & 0x3f] = fontpos;

    return 0;
}

PRIVATE int con_insert_unipair_str ARGS3(
	u16,		unicode,
	CONST char *, 	replace_str,
	int,		fordefault)
{
    int i, n;
    char ***p1;
    CONST char **p2;

    if(fordefault)
	p1 = unidefault_pagedir_str[n = unicode >> 11];
    else
	p1 = uni_pagedir_str[n = unicode >> 11];
    if (!p1) {
	p1 = (char** *)malloc(32*sizeof(char **));
	if (fordefault)
	    unidefault_pagedir_str[n] = p1;
	else
	    uni_pagedir_str[n] = p1;
	if ( !p1 )
	    return -ENOMEM;

	for (i = 0; i < 32; i++) {
	    p1[i] = NULL;
	}
    }

    n = ((unicode >> 6) & 0x1f);
    if (!p1[n]) {
	p1[n] = (char **)malloc(64*sizeof(char *));
	if (!p1[n])
	    return -ENOMEM;

	p2 = (CONST char **)p1[n];
	for (i = 0; i < 64; i++) {
	    p2[i] = NULL;	/* No replace string this character (yet) */
	}
    }
    p2 = (CONST char **)p1[n];

    p2[unicode & 0x3f] = replace_str;

    return 0;
}

/*
 *  ui arg was a leftover, deleted. - KW
 */
PRIVATE void con_clear_unimap ARGS1(
	int,		fordefault)
{
    int i, j;
    u16 **p1;

    if (fordefault) {
	for (i = 0; i < 32; i++) {
	    if ((p1 = unidefault_pagedir[i]) != NULL) {
		for (j = 0; j < 32; j++) {
		    FREE(p1[j]);
		}
		FREE(p1);
	    }
	    unidefault_pagedir[i] = NULL;
	}

	unidefault_contents_valid = 1;
    } else {
	for (i = 0; i < 32; i++) {
	    if ((p1 = uni_pagedir[i]) != NULL) {
		for (j = 0; j < 32; j++) {
		    FREE(p1[j]);
		}
		FREE(p1);
	    }
	    uni_pagedir[i] = NULL;
	}

	hashtable_contents_valid = 1;
    }
}

PRIVATE void con_clear_unimap_str ARGS1(int, fordefault)
{
  int i, j;
  char ***p1;

  if (fordefault) {
    for (i = 0; i < 32; i++) {
	if ((p1 = unidefault_pagedir_str[i]) != NULL) {
	    for (j = 0; j < 32; j++) {
		FREE(p1[j]);
	    }
	    FREE(p1);
	}
      unidefault_pagedir_str[i] = NULL;
    }

    unidefault_str_contents_valid = 1;	/* ??? probably no use... */
  } else {
    for (i = 0; i < 32; i++) {
	if ((p1 = uni_pagedir_str[i]) != NULL) {
	    for (j = 0; j < 32; j++) {
		FREE(p1[j]);
	    }
	    FREE(p1);
	}
      uni_pagedir_str[i] = NULL;
    }

    hashtable_str_contents_valid = 1;  /* ??? probably no use... */
  }
}

#ifdef NOTDEFINED
PRIVATE int con_set_unimap ARGS2(
	u16,			ct,
	struct unipair *,	list)
{
    int err = 0, err1, i;

    while (ct--) {
	if ((err1 = con_insert_unipair(list->unicode, list->fontpos)) != 0) {
	    err = err1;
	}
	list++;
    }

    for (i = 0; i <= 3; i++) {
	set_inverse_transl(i); /* Update all inverse translations */
    }
    return err;
}
#endif /* NOTDEFINED */

/*
 *  Loads the unimap for the hardware font, as defined in uni_hash.tbl.
 *  The representation used was the most compact I could come up
 *  with.  This routine is executed at sys_setup time, and when the
 *  PIO_FONTRESET ioctl is called.
 */
PRIVATE void con_set_default_unimap NOARGS
{
    int i, j;
    CONST u16 *p;

    /*
     *	The default font is always 256 characters.
     *  (default font can not be a fake one, so unicout!=NULL for sure.)
     */
    con_clear_unimap(1);

    p = dfont_unitable;
    for (i = 0; i < 256; i++) {
	for (j = dfont_unicount[i]; j; j--) {
	    con_insert_unipair(*(p++), i, 1);
	}
    }

#if 0
    for (i = 0; i <= 3; i++) {
	set_inverse_transl(i);	/* Update all inverse translations */
    }
#endif

    UC_default_unitable = dfont_unitable;

    con_clear_unimap_str(1);
    UC_con_set_unimap_str(dfont_replacedesc.entry_ct, repl_map, 1);
    UC_default_unitable_str = &dfont_replacedesc;
}

PUBLIC int UCNumCharsets = 0;

PUBLIC int UCLYhndl_HTFile_for_unspec = -1;
PUBLIC int UCLYhndl_HTFile_for_unrec = -1;
PUBLIC int UCLYhndl_for_unspec = -1;
PUBLIC int UCLYhndl_for_unrec = -1;

 /* easy to type, will initialize later */
PUBLIC int LATIN1 = -1;        /* UCGetLYhndl_byMIME("iso-8859-1") */
PUBLIC int US_ASCII = -1;      /* UCGetLYhndl_byMIME("us-ascii")   */
PUBLIC int UTF8 = -1;          /* UCGetLYhndl_byMIME("utf-8")      */


PRIVATE int UC_con_set_unimap ARGS2(
	int,		UC_charset_out_hndl,
	int,		update_flag)
{
    int i, j;
    CONST u16 *p;

    if (!UC_valid_UC_charset(UC_charset_out_hndl)) {
	CTRACE(tfp, "UC_con_set_unimap: Invalid charset handle %d.\n",
		    UC_charset_out_hndl);
	return -1;
    }

    p = UCInfo[UC_charset_out_hndl].unitable;
    if (p == UC_current_unitable) {    /* test whether pointers are equal */
	return update_flag;			/* nothing to be done */
    }
    UC_current_unitable = p;

    /*
     *	The font is always 256 characters - so far.
     *  (fake 0 for built-in charsets like CJK or x-transparent, add a check)
     */
    con_clear_unimap(0);

    for (i = 0; i < 256 && UCInfo[UC_charset_out_hndl].unicount != NULL; i++) {
	for (j = UCInfo[UC_charset_out_hndl].unicount[i]; j; j--) {
	    con_insert_unipair(*(p++), i, 0);
	}
    }

    if (update_flag) {
	for (i = 0; i <= 3; i++) {
	    set_inverse_transl(i);	/* Update all inverse translations */
	}
    }

    return 0;
}

PRIVATE int UC_con_set_unimap_str ARGS3(
	u16,		ct,
	struct unipair_str *, list,
	int,		fordefault)
{
    int err = 0, err1;

    while (ct--) {
	if ((err1 = con_insert_unipair_str(list->unicode,
					   list->replace_str,
					   fordefault)) != 0) {
	    err = err1;
	}
	list++;
    }

    /*
     *	No inverse translations for replacement strings!
     */
    if (!err) {
	if (fordefault)
	    unidefault_str_contents_valid = 1;
	else
	    hashtable_str_contents_valid = 1;
    }

    return err;
}

#ifdef NOTDEFINED
PRIVATE int con_get_unimap ARGS3(
	u16,			ct,
	u16 *,			uct,
	struct unipair *,	list)
{
    int i, j, k, ect;
    u16 **p1, *p2;

    ect = 0;
    if (hashtable_contents_valid) {
	for (i = 0; i < 32; i++) {
	    if ((p1 = uni_pagedir[i]) != NULL) {
		for (j = 0; j < 32; j++) {
		    if ((p2 = *(p1++)) != NULL) {
			for (k = 0; k < 64; k++) {
			    if (*p2 < MAX_GLYPH && ect++ < ct) {
				list->unicode = (u16) ((i<<11)+(j<<6)+k);
				list->fontpos = (u16) *p2;
				list++;
			    }
			    p2++;
			}
		    }
		}
	    }
	}
    }
    *uct = ect;
    return ((ect <= ct) ? 0 : -ENOMEM);
}
#endif /* NOTDEFINED */

PRIVATE int conv_uni_to_pc ARGS2(
	long,		ucs,
	int,		usedefault)
{
    int h;
    u16 **p1, *p2;

    /*
     *	Only 16-bit codes supported at this time.
     */
    if (ucs > 0xffff) {
	/*
	 *  U+FFFD: REPLACEMENT CHARACTER.
	 */
	ucs = 0xfffd;
    } else if (ucs < 0x20 || ucs >= 0xfffe) {
	/*
	 *  Not a printable character.
	 */
	return -1;
    } else if (ucs == 0xfeff || (ucs >= 0x200b && ucs <= 0x200f)) {
	/*
	 *  Zero-width space.
	 */
	return -2;
    } else if ((ucs & ~UNI_DIRECT_MASK) == UNI_DIRECT_BASE) {
	/*
	 *  UNI_DIRECT_BASE indicates the start of the region in the
	 *  User Zone which always has a 1:1 mapping to the currently
	 *  loaded font.  The UNI_DIRECT_MASK indicates the bit span
	 *  of the region.
	 */
	return (ucs & UNI_DIRECT_MASK);
    }

    if (usedefault) {
	if (!unidefault_contents_valid)
	    return -3;
	p1 = unidefault_pagedir[ucs >> 11];
    } else {
	if (!hashtable_contents_valid)
	    return -3;
	p1 = uni_pagedir[ucs >> 11];
    }

    if (p1 &&
	(p2 = p1[(ucs >> 6) & 0x1f]) &&
	(h = p2[ucs & 0x3f]) < MAX_GLYPH) {
	return h;
    }

    /*
     *	Not found.
     */
    return -4;
}

/*
 *  Note: contents of outbuf is not changes for negative return value!
 */
PRIVATE int conv_uni_to_str ARGS4(
	char*,		outbuf,
	int,		buflen,
	long,		ucs,
	int,		usedefault)
{
    char *h;
    char ***p1, **p2;

    /*
     *	Only 16-bit codes supported at this time.
     */
    if (ucs > 0xffff) {
	/*
	 *  U+FFFD: REPLACEMENT CHARACTER.
	 */
	ucs = 0xfffd;
    /*
     *	Maybe the following two cases should be allowed here?? - KW
     */
    } else if (ucs < 0x20 || ucs >= 0xfffe) {
	/*
	 *  Not a printable character.
	 */
	return -1;
    } else if (ucs == 0xfeff || (ucs >= 0x200b && ucs <= 0x200f)) {
	/*
	 *  Zero-width space.
	 */
	return -2;
#ifdef NOTDEFINED	/* We don't handle the following here: */
    } else if ((ucs & ~UNI_DIRECT_MASK) == UNI_DIRECT_BASE) {
	/*
	 *  UNI_DIRECT_BASE indicates the start of the region in the
	 *  User Zone which always has a 1:1 mapping to the currently
	 *  loaded font.  The UNI_DIRECT_MASK indicates the bit span
	 *  of the region.
	 */
	return ucs & UNI_DIRECT_MASK;
#endif /* NOTDEFINED */
    }

    if (usedefault) {
	if (!unidefault_str_contents_valid)
	    return -3;
	p1 = unidefault_pagedir_str[ucs >> 11];
    } else {
	if (!hashtable_str_contents_valid)
	    return -3;
	p1 = uni_pagedir_str[ucs >> 11];
    }

    if (p1 &&
	(p2 = p1[(ucs >> 6) & 0x1f]) &&
	(h = p2[ucs & 0x3f])) {
	strncpy (outbuf,h,(size_t) (buflen-1));
	return 1;     /* ok ! */
    }

    /*
     *	Not found.
     */
    return -4;
}

PUBLIC int UCInitialized = 0;
/*
 *  [ original comment: - KW ]
 * This is called at sys_setup time, after memory and the console are
 * initialized.  It must be possible to call kmalloc(..., GFP_KERNEL)
 * from this function, hence the call from sys_setup.
 */
PRIVATE void UCconsole_map_init NOARGS
{
    con_set_default_unimap();
    UCInitialized = 1;
}

/*
 *  OK now, finally, some stuff that is more specifically for Lynx: - KW
 */
PUBLIC int UCTransUniChar ARGS2(
	long,		unicode,
	int,		charset_out)
{
    int rc = 0;
    int UChndl_out;
    int isdefault, trydefault = 0;
    CONST u16 * ut;

    if ((UChndl_out = LYCharSet_UC[charset_out].UChndl) < 0) {
	if ((UChndl_out = default_UChndl) < 0)
	    return -12;
	isdefault = 1;
    } else {
	isdefault = UCInfo[UChndl_out].replacedesc.isdefault;
	trydefault = UCInfo[UChndl_out].replacedesc.trydefault;
    }

    if (!isdefault) {
	ut = UCInfo[UChndl_out].unitable;
	if (ut != UC_current_unitable) {
	    rc = UC_con_set_unimap(UChndl_out, 1);
	    if (rc < 0) {
		return rc;
	    }
	}
	rc = conv_uni_to_pc(unicode, 0);
	if (rc >= 0)
	    return rc;
    }
    if (isdefault || trydefault) {
	rc = conv_uni_to_pc(unicode, 1);
	if (rc >= 0)
	    return rc;
    }
    if (!isdefault && (rc == -4)) {
	rc = conv_uni_to_pc(0xfffd, 0);
    }
    if ((isdefault || trydefault) && (rc == -4)) {
	rc = conv_uni_to_pc(0xfffd, 1);
    }
    return rc;
}

/*
 *  Returns string length, or negative value for error.
 */
PUBLIC int UCTransUniCharStr ARGS5(
	char *, 	outbuf,
	int,		buflen,
	long,		unicode,
	int,		charset_out,
	int,		chk_single_flag)
{
    int rc = -14, src = 0, ignore_err;
    int UChndl_out;
    int isdefault, trydefault = 0;
    struct unimapdesc_str * repl;
    CONST u16 * ut;

    if (buflen < 2)
	return -13;

    if ((UChndl_out = LYCharSet_UC[charset_out].UChndl) < 0) {
	if ((UChndl_out = default_UChndl) < 0)
	    return -12;
	isdefault = 1;
    } else {
	isdefault = UCInfo[UChndl_out].replacedesc.isdefault;
	trydefault = UCInfo[UChndl_out].replacedesc.trydefault;
    }

    if (chk_single_flag) {
	if (!isdefault) {
	    ut = UCInfo[UChndl_out].unitable;
	    if (ut != UC_current_unitable) {
		src = UC_con_set_unimap(UChndl_out, 1);
		if (src < 0) {
		    return src;
		}
	    }
	}
	src = conv_uni_to_pc(unicode, isdefault);
	if (src >= 32) {
	    outbuf[0] = src; outbuf[1] = '\0';
	    return 1;
	}
    }

    repl = &(UCInfo[UChndl_out].replacedesc);
    if (!isdefault) {
	if (repl != UC_current_unitable_str)  {
	    con_clear_unimap_str(0);
	    ignore_err = UC_con_set_unimap_str(repl->entry_ct, repl->entries, 0);
	    UC_current_unitable_str = repl;
	}
	rc = conv_uni_to_str(outbuf, buflen, unicode, 0);
	if (rc >= 0)
	    return (strlen(outbuf));
    }
    if (trydefault && chk_single_flag) {
	src = conv_uni_to_pc(unicode, 1);
	if (src >= 32) {
	    outbuf[0] = src; outbuf[1] = '\0';
	    return 1;
	}
    }
    if (isdefault || trydefault) {
	rc = conv_uni_to_str(outbuf, buflen, unicode, 1);
	if (rc >= 0)
	    return (strlen(outbuf));
    }
    if (rc == -4) {
	if (!isdefault)
	    rc = conv_uni_to_str(outbuf, buflen, 0xfffd, 0);
	if ((rc == -4) && (isdefault || trydefault))
	    rc = conv_uni_to_str(outbuf, buflen, 0xfffd, 1);
	if (rc >= 0)
	    return (strlen(outbuf));
    }
    if (chk_single_flag && src == -4) {
	if (!isdefault)
	    rc = conv_uni_to_pc(0xfffd, 0);
	if ((rc == -4) && (isdefault || trydefault))
	    rc = conv_uni_to_pc(0xfffd, 1);
	if (rc >= 32) {
	    outbuf[0] = rc; outbuf[1] = '\0';
	    return 1;
	}
	return rc;
    }
    return -4;
}

PRIVATE int UC_lastautoGN = 0;

PRIVATE int UC_MapGN ARGS2(
	int,		UChndl,
	int,		update_flag)
{
    int i, Gn, found, lasthndl;
    found = 0;
    Gn = -1;
    for (i = 0; i < 4 && Gn < 0; i++) {
	if (UC_GNhandles[i] < 0) {
	    Gn = i;
	} else if (UC_GNhandles[i] == UChndl) {
	    Gn = i;
	    found = 1;
	}
    }
    if (found)
	return Gn;
    if (Gn >= 0) {
	UCInfo[UChndl].GN = Gn;
	UC_GNhandles[Gn] = UChndl;
    } else {
	if (UC_lastautoGN == GRAF_MAP) {
	    Gn = IBMPC_MAP;
	} else {
	    Gn = GRAF_MAP;
	}
	UC_lastautoGN = Gn;
	lasthndl = UC_GNhandles[Gn];
	UCInfo[lasthndl].GN = -1;
	UCInfo[UChndl].GN = Gn;
	UC_GNhandles[Gn] = UChndl;
    }
    CTRACE(tfp, "UC_MapGN: Using %d <- %d (%s)\n",
		Gn, UChndl, UCInfo[UChndl].MIMEname);
    UC_con_set_trans(UChndl,Gn,update_flag);
    return Gn;
}

PUBLIC int UCTransChar ARGS3(
	char,		ch_in,
	int,		charset_in,
	int,		charset_out)
{
    int unicode, Gn;
    int rc = -4;
    int UChndl_in, UChndl_out;
    int isdefault, trydefault = 0;
    CONST u16 * ut;
    int upd = 0;

#ifndef UC_NO_SHORTCUTS
    if (charset_in == charset_out)
	return (unsigned char)ch_in;
#endif /* UC_NO_SHORTCUTS */
    if (charset_in < 0)
	return -11;
    if ((UChndl_in = LYCharSet_UC[charset_in].UChndl) < 0)
	return -11;
    if ((UChndl_out = LYCharSet_UC[charset_out].UChndl) < 0) {
	if ((UChndl_out = default_UChndl) < 0)
	    return -12;
	isdefault = 1;
    } else {
	isdefault = UCInfo[UChndl_out].replacedesc.isdefault;
	trydefault = UCInfo[UChndl_out].replacedesc.trydefault;
    }
    if (!UCInfo[UChndl_in].num_uni)
	return -11;
    if ((Gn = UCInfo[UChndl_in].GN) < 0) {
	Gn = UC_MapGN(UChndl_in,0);
	upd = 1;
    }

    ut = UCInfo[UChndl_out].unitable;
    if (!isdefault) {
	if (ut == UC_current_unitable) {
	    if (upd) {
		set_inverse_transl(Gn);
	    }
	} else {
	    rc = UC_con_set_unimap(UChndl_out, 1);
	    if (rc > 0) {
		set_inverse_transl(Gn);
	    } else if (rc < 0) {
		return rc;
	    }
	}
    }
    UC_translate = set_translate(Gn);
    unicode = UC_translate[(unsigned char)ch_in];
    if (!isdefault) {
	rc = conv_uni_to_pc(unicode, 0);
	if (rc >= 0)
	    return rc;
    }
    if ((rc == -4) && (isdefault || trydefault)) {
	rc = conv_uni_to_pc(unicode, 1);
    }
    if ((rc == -4) && !isdefault) {
	rc = conv_uni_to_pc(0xfffd, 0);
    }
    if ((rc == -4) && (isdefault || trydefault)) {
	rc = conv_uni_to_pc(0xfffd, 1);
    }
    return rc;
}

PUBLIC long int UCTransToUni ARGS2(
	char,		ch_in,
	int,		charset_in)
{
  int unicode, Gn;
  unsigned char ch_iu;
  int UChndl_in;

  ch_iu = (unsigned char)ch_in;
#ifndef UC_NO_SHORTCUTS
    if (charset_in == LATIN1)
	return ch_iu;
    if ((unsigned char)ch_in < 128 && (unsigned char)ch_in >= 32)
	return ch_iu;
#endif /* UC_NO_SHORTCUTS */
    if (charset_in < 0)
	return -11;
    if ((unsigned char)ch_in < 32 &&
	LYCharSet_UC[charset_in].enc != UCT_ENC_8BIT_C0)
	/*
	 *  Don't translate C0 chars except for specific charsets.
	 */
	return ch_iu;
    if ((UChndl_in = LYCharSet_UC[charset_in].UChndl) < 0)
	return -11;
    if (!UCInfo[UChndl_in].num_uni)
	return -11;
    if ((Gn = UCInfo[UChndl_in].GN) < 0) {
	Gn = UC_MapGN(UChndl_in,1);
    }

  UC_translate = set_translate(Gn);
  unicode = UC_translate[(unsigned char)ch_in];

  return unicode;
}

PUBLIC int UCReverseTransChar ARGS3(
	char,		ch_out,
	int,		charset_in,
	int,		charset_out)
{
    int Gn;
    int rc;
    int UChndl_in, UChndl_out;
    int isdefault;
    int i_ch = (unsigned char)ch_out;
    CONST u16 * ut;

#ifndef UC_NO_SHORTCUTS
    if (charset_in == charset_out)
	return ch_out;
#endif /* UC_NO_SHORTCUTS */
    if (charset_in < 0)
	return -11;
    if ((UChndl_in = LYCharSet_UC[charset_in].UChndl) < 0)
	return -11;
    if (!UCInfo[UChndl_in].num_uni)
	return -11;
    if (charset_out < 0)
	return -12;
    if ((UChndl_out = LYCharSet_UC[charset_out].UChndl) < 0) {
	if ((UChndl_out = default_UChndl) < 0)
	    return -12;
	isdefault = 1;
    } else {
	isdefault = UCInfo[UChndl_out].replacedesc.isdefault;
    }

    if (!isdefault) {
	/*
	 *  Try to use the inverse table if charset_out is not equivalent
	 *  to using just the default table.  If it is, it should have
	 *  just ASCII chars and trying to back-translate those should
	 *  not give anything but themselves. - kw
	 */
	ut = UCInfo[UChndl_out].unitable;
	if (ut == UC_current_unitable) {
	    if ((Gn = UCInfo[UChndl_in].GN) >= 0) {
		UC_translate = set_translate(Gn);
		rc = inv_translate[i_ch];
		if (rc >= 32) {
		    return rc;
		}
	    } else {
		Gn = UC_MapGN(UChndl_in,1);
		UC_translate = set_translate(Gn);
		rc = inv_translate[i_ch];
		if (rc >= 32) {
		    return rc;
		}
	    }
	}
    }
    return UCTransChar(ch_out, charset_out, charset_in);
}

/*
 *  Returns string length, or negative value for error.
 */
PUBLIC int UCTransCharStr ARGS6(
	char *, 	outbuf,
	int,		buflen,
	char,		ch_in,
	int,		charset_in,
	int,		charset_out,
	int,		chk_single_flag)
{
    int unicode, Gn;
    int rc = -14, src = 0, ignore_err;
    int UChndl_in, UChndl_out;
    int isdefault, trydefault = 0;
    struct unimapdesc_str * repl;
    CONST u16 * ut;
    int upd = 0;

    if (buflen < 2)
	return -13;
#ifndef UC_NO_SHORTCUTS
    if (chk_single_flag && charset_in == charset_out) {
	outbuf[0] = ch_in;
	outbuf[1] = '\0';
	return 1;
    }
#endif /* UC_NO_SHORTCUTS */
    if (charset_in < 0)
	return -11;
    if ((UChndl_in = LYCharSet_UC[charset_in].UChndl) < 0)
	return -11;
    if (!UCInfo[UChndl_in].num_uni)
	return -11;
    if ((UChndl_out = LYCharSet_UC[charset_out].UChndl) < 0) {
	if ((UChndl_out = default_UChndl) < 0)
	    return -12;
	isdefault = 1;
    } else {
	isdefault = UCInfo[UChndl_out].replacedesc.isdefault;
	trydefault = UCInfo[UChndl_out].replacedesc.trydefault;
    }
    if ((Gn = UCInfo[UChndl_in].GN) < 0) {
	Gn = UC_MapGN(UChndl_in, !chk_single_flag);
	upd = chk_single_flag;
    }

    UC_translate = set_translate(Gn);
    unicode = UC_translate[(unsigned char)ch_in];

    if (chk_single_flag) {
	if (!isdefault) {
	    ut = UCInfo[UChndl_out].unitable;
	    if (ut == UC_current_unitable) {
		if (upd) set_inverse_transl(Gn);
	    } else {
		src = UC_con_set_unimap(UChndl_out, 1);
		if (src > 0) {
		    set_inverse_transl(Gn);
		} else if (src < 0) {
		    return src;
		}
	    }
	}
	src = conv_uni_to_pc(unicode, isdefault);
	if (src >= 32) {
	    outbuf[0] = src; outbuf[1] = '\0';
	    return 1;
	}
    }

    repl = &(UCInfo[UChndl_out].replacedesc);
    if (!isdefault) {
	if (repl != UC_current_unitable_str) {
	    con_clear_unimap_str(0);
	    ignore_err = UC_con_set_unimap_str(repl->entry_ct, repl->entries, 0);
	    UC_current_unitable_str = repl;
	}
	rc = conv_uni_to_str(outbuf, buflen, unicode, 0);
	if (rc >= 0)
	    return (strlen(outbuf));
    }
    if (trydefault && chk_single_flag) {
	src = conv_uni_to_pc(unicode, 1);
	if (src >= 32) {
	    outbuf[0] = src; outbuf[1] = '\0';
	    return 1;
	}
    }
    if (isdefault || trydefault) {
	rc = conv_uni_to_str(outbuf, buflen, unicode, 1);
	if (rc >= 0)
	    return (strlen(outbuf));
    }
    if (rc == -4) {
	if (!isdefault)
	    rc = conv_uni_to_str(outbuf, buflen, 0xfffd, 0);
	if ((rc == -4) && (isdefault || trydefault))
	    rc = conv_uni_to_str(outbuf, buflen, 0xfffd, 1);
	if (rc >= 0)
	    return (strlen(outbuf));
    }
    if (chk_single_flag && src == -4) {
	if (!isdefault)
	    rc = conv_uni_to_pc(0xfffd, 0);
	if ((rc == -4) && (isdefault || trydefault))
	    rc = conv_uni_to_pc(0xfffd, 1);
	if (rc >= 32) {
	    outbuf[0] = rc; outbuf[1] = '\0';
	    return 1;
	} else if (rc <= 0) {
	    outbuf[0] = '\0';
	    return rc;
	}
	return rc;
    }
    return -4;
}

PRIVATE int UC_FindGN_byMIME ARGS1(
	CONST char *,	UC_MIMEcharset)
{
  int i;

    for (i = 0; i < 4; i++) {
	if (!strcmp(UC_MIMEcharset,UC_GNsetMIMEnames[i])) {
      return i;
	}
    }
  return -1;
}

PUBLIC int UCGetRawUniMode_byLYhndl ARGS1(
	int,		i)
{
    if (i < 0)
	return 0;
  return LYCharSet_UC[i].enc;
}

/*
 *  Get Lynx internal charset handler from MIME name,
 *  return -1 if we got NULL or did not recognize value.
 *  According to RFC, MIME headers should match case-insensitively.
 */
PUBLIC int UCGetLYhndl_byMIME ARGS1(
	CONST char *,	value)
{
    int i;
    int LYhndl = -1;

    if (!value || !(*value)) {
	CTRACE(tfp, "UCGetLYhndl_byMIME: NULL argument instead of MIME name.\n");
	return -1;
    }

    for (i = 0;
	 (i < MAXCHARSETS && i < LYNumCharsets &&
	  LYchar_set_names[i]); i++) {
	if (LYCharSet_UC[i].MIMEname &&
	   !strcasecomp(value, LYCharSet_UC[i].MIMEname)) {
	    return i;
	}
    }

    /*
     * Not yet found, try synonyms.  - FM
     */
    if (!strcasecomp(value, "unicode-1-1-utf-8") ||
	!strcasecomp(value, "utf8")) {
	/*
	 * Treat these as synonyms for the IANA registered name.  - FM
	 */
	return UCGetLYhndl_byMIME("utf-8");
    }
    if (!strncasecomp(value, "iso-2022-jp", 11) ||
	!strcasecomp(value, "x-euc-jp")) {
	return UCGetLYhndl_byMIME("euc-jp");
    }
    if (!strcasecomp(value, "x-shift-jis")) {
	return UCGetLYhndl_byMIME("shift_jis");
    }
    if (!strcasecomp(value, "iso-2022-kr")) {
	return UCGetLYhndl_byMIME("euc-kr");
    }
    if (!strcasecomp(value, "gb2312") ||
	!strncasecomp(value, "cn-gb", 5) ||
	!strcasecomp(value, "iso-2022-cn")) {
	return UCGetLYhndl_byMIME("euc-cn");
    }
    if (!strcasecomp(value, "cn-big5")) {
	return UCGetLYhndl_byMIME("big5");
    }
    if (!strcasecomp(value, "x-mac-roman") ||
	!strcasecomp(value, "mac-roman")) {
	return UCGetLYhndl_byMIME("macintosh");
    }
    if (!strcasecomp(value, "x-next") ||
	!strcasecomp(value, "nextstep") ||
	!strcasecomp(value, "x-nextstep")) {
	return UCGetLYhndl_byMIME("next");
    }
    if (!strcasecomp(value, "iso-8859-1-windows-3.1-latin-1") ||
	!strcasecomp(value, "cp1252") ||
	!strcasecomp(value, "cp-1252") ||
	!strcasecomp(value, "ibm1252") ||
	!strcasecomp(value, "iso-8859-1-windows-3.0-latin-1")) {
	/*
	 * Treat these as synonyms for windows-1252, which is more
	 * commonly used than the IANA registered name.  - FM
	 */
	return UCGetLYhndl_byMIME("windows-1252");
    }
    if (!strcasecomp(value, "iso-8859-2-windows-latin-2") ||
	!strcasecomp(value, "cp1250") ||
	!strcasecomp(value, "cp-1250") ||
	!strcasecomp(value, "ibm1250")) {
	/*
	 * Treat these as synonyms for windows-1250.  - FM
	 */
	return UCGetLYhndl_byMIME("windows-1250");
    }
    if ((!strncasecomp(value, "ibm", 3) ||
	 !strncasecomp(value, "cp-", 3)) &&
	isdigit((unsigned char)value[3]) &&
	isdigit((unsigned char)value[4]) &&
	isdigit((unsigned char)value[5])) {
	/*
	 * For "ibmNNN<...>" or "cp-NNN", try "cpNNN<...>"
	 * if not yet found.  - KW & FM
	 */
	char * cptmp = NULL;

	StrAllocCopy(cptmp, (value + 1));
	cptmp[0] = 'c';
	cptmp[1] = 'p';
	if ((LYhndl = UCGetLYhndl_byMIME(cptmp)) >= 0) {
	    FREE(cptmp);
	    return LYhndl;
	}
	/*
	 * Try windows-NNN<...> if not yet found.  - FM
	 */
	StrAllocCopy(cptmp, "windows-");
	StrAllocCat(cptmp, (value + 3));
	LYhndl = UCGetLYhndl_byMIME(cptmp);
	FREE(cptmp);
	return LYhndl;
    }
    if (!strncasecomp(value, "windows-", 8) &&
	isdigit((unsigned char)value[8]) &&
	isdigit((unsigned char)value[9]) &&
	isdigit((unsigned char)value[10])) {
	/*
	 * For "windows-NNN<...>", try "cpNNN<...>" - FM
	 */
	char * cptmp = NULL;

	StrAllocCopy(cptmp, (value + 6));
	cptmp[0] = 'c';
	cptmp[1] = 'p';
	LYhndl = UCGetLYhndl_byMIME(cptmp);
	FREE(cptmp);
	return LYhndl;
    }
    if (!strcasecomp(value, "koi-8")) { /* accentsoft bugosity */
      return UCGetLYhndl_byMIME("koi8-r");
    }
    /* no more synonyms if come here... */

    CTRACE(tfp, "UCGetLYhndl_byMIME: unrecognized MIME name \"%s\"\n", value);
    return -1;	/* returns -1 if no charset found by that MIME name */
}

/*
 *  Function UC_setup_LYCharSets_repl() tries to set up a subtable in
 *  LYCharSets[] appropriate for this new charset, for compatibility
 *  with the "old method".  Maybe not nice (maybe not even necessary
 *  any more), but it works (as far as it goes..).
 *
 *  We try to be conservative and only allocate new memory for this
 *  if needed.	If not needed, just point to SevenBitApproximations[i].
 *  [Could do the same for ISO_Latin1[] if it's identical to that, but
 *   would make it even *more* messy than it already is...]
 *  This the only function in this file that knows, or cares, about the
 *  HTMLDTD or details of LYCharSets[] subtables (and therefore somewhat
 *  violates the idea that this file should be independent of those).
 *  As in other places, we rely on ISO_Latin1 being the *first* table
 *  in LYCharSets. - KW
 */

/*
 *  We need to remember which ones were allocated and which are static.
 */
PRIVATE CONST char ** remember_allocated_LYCharSets[MAXCHARSETS];

PRIVATE void UCreset_allocated_LYCharSets NOARGS
{
    int i = 0;

    for (; i < MAXCHARSETS; i++) {
	remember_allocated_LYCharSets[i] = NULL;
    }
}

#ifdef LY_FIND_LEAKS
PRIVATE void UCfree_allocated_LYCharSets NOARGS
{
    int i = 0;

    for (; i < MAXCHARSETS; i++) {
	if (remember_allocated_LYCharSets[i] != NULL) {
	    FREE(remember_allocated_LYCharSets[i]);
	}
    }
}
#endif

PRIVATE CONST char ** UC_setup_LYCharSets_repl ARGS2(
	int,		UC_charset_in_hndl,
	unsigned,	lowest8)
{
    CONST char **ISO_Latin1 = LYCharSets[0];
    CONST char **p;
    char **prepl;
    CONST u16 *pp;
    CONST char **tp;
    CONST char *s7;
    CONST char *s8;
    size_t i;
    int j, changed;
    u16 k;
    u8 *ti;

    /*
     *	Create a temporary table for reverse lookup of latin1 codes:
     */
    tp = (CONST char **)malloc(96 * sizeof(CONST char *));
    if (!tp)
	return NULL;
    for (i = 0; i < 96; i++)
	tp[i] = NULL;
    ti = (u8 *)malloc(96 * sizeof(u8));
    if (!ti) {
	FREE(tp);
	return NULL;
    }
    for (i = 0; i < 96; i++)
	ti[i] = 0;

    pp = UCInfo[UC_charset_in_hndl].unitable;

    /*
     *	Determine if we have any mapping of a Unicode in the range 160-255
     *	to an allowed code point > 0x80 in our new charset...
     *	Store any mappings found in ti[].
     */
    if (UCInfo[UC_charset_in_hndl].num_uni > 0) {
	for (i = 0; i < 256; i++) {
	    if ((j = UCInfo[UC_charset_in_hndl].unicount[i])) {
		if ((k = *pp) >= 160 && k < 256 && i >= lowest8) {
		   ti[k-160] = i;
		}
		for (; j; j--) {
		    pp++;
		}
	    }
	}
    }
    {
	u16 ct;
	struct unipair_str *list;

	/*
	 *  Determine if we have any mapping of a Unicode in the range
	 *  160-255 to a replacement string for our new charset...
	 *  Store any mappings found in tp[].
	 */
	ct = UCInfo[UC_charset_in_hndl].replacedesc.entry_ct;
	list = UCInfo[UC_charset_in_hndl].replacedesc.entries;
	while (ct--) {
	    if ((k = list->unicode) >= 160 && k < 256) {
		tp[k-160] = list->replace_str;
	    }
	    list++;
	}
    }
    /*
     *	Now allocate a new table compatible with LYCharSets[]
     *	and with the HTMLDTD for entities.
     *	We don't know yet whether we'll keep it around. */
    prepl = (char **)malloc(HTML_dtd.number_of_entities * sizeof(char *));
    if (!prepl) {
	FREE(tp);
	FREE(ti);
	return 0;
    }

    p = (CONST char **)prepl;
    changed = 0;
    for (i = 0; i < HTML_dtd.number_of_entities; i++, p++) {
	/*
	 *  For each of those entities, we check what the "old method"
	 *  ISO_Latin1[] mapping does with them.  If it is nothing we
	 *  want to use, just point to the SevenBitApproximations[] string.
	 */
	s7 = SevenBitApproximations[i];
	s8 = ISO_Latin1[i];
	*p = s7;
	if (s8 && (unsigned char)(*s8) >= 160 && strlen(s8) == 1) {
	    /*
	     *	We have an entity that is mapped to
	     *	one valid eightbit latin1 char.
	     */
	    if (ti[(unsigned char)(*s8) - 160] >= lowest8 &&
		!(s7[0] == ti[(unsigned char)(*s8) - 160] &&
		s7[1] == '\0')) {
		/*
		 *  ...which in turn is mapped, by our "new method",
		 *   to another valid eightbit char for this new
		 *   charset: either to itself...
		 */
		if (ti[(unsigned char)(*s8) - 160] == (unsigned char)(*s8)) {
		    *p = s8;
		} else {
		    /*
		     *			      ...or another byte...
		     */
#ifdef NOTDEFINED
		    *p = (char *)malloc(2*sizeof(char));
		    if (!*p) {
			FREE(tp);
			FREE(ti);
			FREE(prepl);
			return NULL;
		    }
		    (*p)[0] = ti[(unsigned char)(*s8) - 160];
		    (*p)[1] = '\0';
#else
		    /*
		     *	Use this instead... make those 1-char strings
		     *	into HTAtoms, so they will be cleaned up
		     *	at exit... all for the sake of preventing
		     *	memory leaks, sigh.
		     */
		    static char dummy[2];	/* one char dummy string */

		    dummy[0] = ti[(unsigned char)(*s8) - 160];
		    *p = HTAtom_name(HTAtom_for(dummy));
#endif /* NOTDEFINED */
		}
		changed = 1;
	    } else if (tp[(unsigned char)(*s8) - 160] &&
		       strcmp(s7, tp[(unsigned char)(*s8) - 160])) {
		/*
		 *  ...or which is mapped, by our "new method",
		 *  to a replacement string for this new charset.
		 */
		*p = tp[(unsigned char)(*s8) - 160];
		changed = 1;
	    }
	}
    }
    FREE(tp);
    FREE(ti);
    if (!changed) {
	FREE(prepl);
	return NULL;
    }
    return (CONST char **)prepl;
}

/*
 *  "New method" meets "Old method" ...
 */
PRIVATE int UC_Register_with_LYCharSets ARGS4(
	int,		s,
	CONST char *,	UC_MIMEcharset,
	CONST char *,	UC_LYNXcharset,
	int,		lowest_eightbit)
{
    int i, LYhndl, found;
    CONST char **repl;

    LYhndl = -1;
    if (LYNumCharsets == 0) {
	/*
	 *  Initialize here; so whoever changes
	 *  LYCharSets.c doesn't have to count...
	 */
	for (i = 0; (i < MAXCHARSETS) && LYchar_set_names[i]; i++) {
	    LYNumCharsets = i+1;
	}
    }

    /*
     *	Search by MIME name, (LYchar_set_names may differ...)
     */
    for (i = 0; i < MAXCHARSETS && LYchar_set_names[i] && LYhndl < 0; i++) {
	if (LYCharSet_UC[i].MIMEname &&
	    !strcmp(UC_MIMEcharset, LYCharSet_UC[i].MIMEname)) {
	    LYhndl = i;
	}
    }

    if (LYhndl < 0) {		/* not found */
	found = 0;
	if (LYNumCharsets >= MAXCHARSETS) {
	    CTRACE(tfp, "UC_Register_with_LYCharSets: Too many.  Ignoring %s/%s.",
			UC_MIMEcharset, UC_LYNXcharset);
	    return -1;
	}
	/*
	 *  Add to LYCharSets.c lists.
	 */
	LYhndl = LYNumCharsets;
	LYNumCharsets ++;
	LYlowest_eightbit[LYhndl] = 999;
	LYCharSets[LYhndl] = SevenBitApproximations;
	/*
	 *  Hmm, try to be conservative here.
	 */
	LYchar_set_names[LYhndl] = UC_LYNXcharset;
	LYchar_set_names[LYhndl+1] = NULL;
	/*
	*  Terminating NULL may be looked for by Lynx code.
	*/
    } else {
	found = 1;
    }
    LYCharSet_UC[LYhndl].UChndl = s;
    /*
     *	Can we just copy the pointer?  Hope so...
     */
    LYCharSet_UC[LYhndl].MIMEname = UC_MIMEcharset;
    LYCharSet_UC[LYhndl].enc = UCInfo[s].enc;
    LYCharSet_UC[LYhndl].codepage = UCInfo[s].codepage;

    /*
     *	@@@ We really SHOULD get more info from the table files,
     *	and set relevant flags in the LYCharSet_UC[] entry with
     *	that info...  For now, let's try it without. - KW
     */
    if (lowest_eightbit < LYlowest_eightbit[LYhndl]) {
	LYlowest_eightbit[LYhndl] = lowest_eightbit;
    } else if (lowest_eightbit > LYlowest_eightbit[LYhndl]) {
	UCInfo[s].lowest_eight = LYlowest_eightbit[LYhndl];
    }

    if (!found && LYhndl > 0) {
	repl = UC_setup_LYCharSets_repl(s,UCInfo[s].lowest_eight);
	if (repl) {
	    LYCharSets[LYhndl] = repl;
	    /*
	     *	Remember to FREE at exit.
	     */
	    remember_allocated_LYCharSets[LYhndl] = repl;
	}
    }
    return LYhndl;
}

/*
 *  This only sets up the structure - no initialization of the tables
 * is done here yet.
 */
PUBLIC void UC_Charset_Setup ARGS9(
	CONST char *,		UC_MIMEcharset,
	CONST char *,		UC_LYNXcharset,
	CONST u8 *,		unicount,
	CONST u16 *,		unitable,
	int,			nnuni,
	struct unimapdesc_str,	replacedesc,
	int,			lowest_eight,
	int,			UC_rawuni,
	int,			codepage)
{
    int s, Gn;
    int i, status = 0, found;

    /*
     *	Get (new?) slot.
     */
    found = -1;
    for (i = 0; i < UCNumCharsets && found < 0; i++) {
	if (!strcmp(UCInfo[i].MIMEname, UC_MIMEcharset)) {
	    found = i;
	}
    }
    if (found >= 0) {
	s = found;
    } else {
	if (UCNumCharsets >= MAXCHARSETS) {
	    CTRACE(tfp, "UC_Charset_Setup: Too many.  Ignoring %s/%s.",
			UC_MIMEcharset, UC_LYNXcharset);
	    return;
	}
	s = UCNumCharsets;
	UCInfo[s].MIMEname = UC_MIMEcharset;
    }
    UCInfo[s].LYNXname = UC_LYNXcharset;
    UCInfo[s].unicount = unicount;
    UCInfo[s].unitable = unitable;
    UCInfo[s].num_uni = nnuni;
    UCInfo[s].replacedesc = replacedesc;
    if (replacedesc.isdefault) {
	default_UChndl = s;
    }
    Gn = UC_FindGN_byMIME(UC_MIMEcharset);
    if (Gn >= 0)
	UC_GNhandles[Gn] = s;
    UCInfo[s].GN = Gn;
    if (UC_rawuni == UCT_ENC_UTF8)
	lowest_eight = 128;  /* cheat here */
    UCInfo[s].lowest_eight = lowest_eight;
    UCInfo[s].enc = UC_rawuni;
    UCInfo[s].codepage = codepage;
    UCInfo[s].LYhndl = UC_Register_with_LYCharSets(s,
						   UC_MIMEcharset,
						   UC_LYNXcharset,
						   lowest_eight);
    UCInfo[s].uc_status = status;
    if (found < 0)
	UCNumCharsets++;
    return;
}

#ifdef LY_FIND_LEAKS
PRIVATE void UCcleanup_mem NOARGS
{
    int i;

    UCfree_allocated_LYCharSets();
    con_clear_unimap_str(0);
    con_clear_unimap_str(1);
    con_clear_unimap(0);
    con_clear_unimap(1);
    for (i = 1; i < 4; i++) {	/* first one is static! */
	FREE(inverse_translations[i]);
    }
}
#endif /* LY_FIND_LEAKS */

PUBLIC void UCInit NOARGS
{

    UCreset_allocated_LYCharSets();
#ifdef LY_FIND_LEAKS
    atexit(UCcleanup_mem);
#endif
    UCconsole_map_init();

/*
 *  The order of charset names visible in Lynx Options menu
 *  correspond to the order of lines below,
 *  except the first two described in LYCharSet.c
 *
 *  Entries whose comment is marked with *** are declared in UCdomap.h,
 *  others are based on the included tables - UCdomap.c, near the top.
 */

    UC_CHARSET_SETUP_iso_8859_1;	  /* ISO Latin 1	  */
    UC_CHARSET_SETUP_iso_8859_15;	  /* ISO 8859-15 (Latin 9)*/
    UC_CHARSET_SETUP_cp850;		  /* DosLatin1 (cp850)	  */
    UC_CHARSET_SETUP_windows_1252;	  /* WinLatin1 (cp1252)   */
    UC_CHARSET_SETUP_cp437;		  /* DosLatinUS (cp437)   */

    UC_CHARSET_SETUP_dec_mcs;		  /* DEC Multinational	  */
    UC_CHARSET_SETUP_macintosh; 	  /* Macintosh (8 bit)	  */
    UC_CHARSET_SETUP_next;		  /* NeXT character set   */
    UC_CHARSET_SETUP_hp_roman8;		  /* HP Roman8		  */

    UC_CHARSET_SETUP_euc_cn;		  /*** Chinese		    */
    UC_CHARSET_SETUP_euc_jp;		  /*** Japanese (EUC_JP)    */
    UC_CHARSET_SETUP_shift_jis; 	  /*** Japanese (Shift_JIS) */
    UC_CHARSET_SETUP_euc_kr;		  /*** Korean		    */
    UC_CHARSET_SETUP_big5;		  /*** Taipei (Big5)	    */

    UC_CHARSET_SETUP_viscii;		  /* Vietnamese (VISCII)  */
    UC_CHARSET_SETUP;	/* us-ascii */	  /* 7 bit approximations */

    UC_CHARSET_SETUP_x_transparent;	  /*** Transparent	  */

    UC_CHARSET_SETUP_iso_8859_2;	  /* ISO Latin 2	  */
    UC_CHARSET_SETUP_cp852;		  /* DosLatin2 (cp852)	  */
    UC_CHARSET_SETUP_windows_1250;	  /* WinLatin2 (cp1250)   */
    UC_CHARSET_SETUP_iso_8859_3;	  /* ISO Latin 3	  */
    UC_CHARSET_SETUP_iso_8859_4;	  /* ISO Latin 4	  */
    UC_CHARSET_SETUP_cp775;		  /* DosBaltRim (cp775)   */
    UC_CHARSET_SETUP_windows_1257;	  /* WinBaltRim (cp1257)  */
    UC_CHARSET_SETUP_iso_8859_5;	  /* ISO 8859-5 Cyrillic  */
    UC_CHARSET_SETUP_cp866;		  /* DosCyrillic (cp866)  */
    UC_CHARSET_SETUP_windows_1251;	  /* WinCyrillic (cp1251) */
    UC_CHARSET_SETUP_koi8_r;		  /* KOI8-R Cyrillic	  */
    UC_CHARSET_SETUP_iso_8859_6;	  /* ISO 8869-6 Arabic	  */
    UC_CHARSET_SETUP_cp864;		  /* DosArabic (cp864)	  */
    UC_CHARSET_SETUP_windows_1256;	  /* WinArabic (cp1256)   */
    UC_CHARSET_SETUP_iso_8859_7;	  /* ISO 8859-7 Greek	  */
    UC_CHARSET_SETUP_cp737;		  /* DosGreek (cp737)	  */
    UC_CHARSET_SETUP_cp869;		  /* DosGreek2 (cp869)	  */
    UC_CHARSET_SETUP_windows_1253;	  /* WinGreek (cp1253)	  */
    UC_CHARSET_SETUP_iso_8859_8;	  /* ISO 8859-8 Hebrew	  */
    UC_CHARSET_SETUP_cp862;		  /* DosHebrew (cp862)	  */
    UC_CHARSET_SETUP_windows_1255;	  /* WinHebrew (cp1255)   */
    UC_CHARSET_SETUP_iso_8859_9;	  /* ISO 8859-9 (Latin 5) */
    UC_CHARSET_SETUP_iso_8859_10;	  /* ISO 8859-10	  */

    UC_CHARSET_SETUP_utf_8;		  /*** UNICODE UTF-8	  */
    UC_CHARSET_SETUP_mnemonic_ascii_0;	  /* RFC 1345 w/o Intro   */
    UC_CHARSET_SETUP_mnemonic;		  /* RFC 1345 Mnemonic	  */
    UC_CHARSET_SETUP_cp866u;		  /* Ukrainian Cyrillic (866) */
    UC_CHARSET_SETUP_koi8_u;		  /* Ukrainian Cyrillic (koi8-u) */
#ifdef NOTDEFINED
    UC_CHARSET_SETUP_mnem;
#endif /* NOTDEFINED */

/*
 *  To add synonyms for any charset name
 *  check function UCGetLYhndl_byMIME in this file.
 */

/* easy to type: */
    LATIN1   = UCGetLYhndl_byMIME("iso-8859-1");
    US_ASCII = UCGetLYhndl_byMIME("us-ascii");
    UTF8     = UCGetLYhndl_byMIME("utf-8");
}

/*
 *  Safe variant of UCGetLYhndl_byMIME, with blind recovery from typo
 *  in user input: lynx.cfg, userdefs.h, switches from command line.
 */
PUBLIC int safeUCGetLYhndl_byMIME ARGS1 (CONST char *, value)
{
    int i = UCGetLYhndl_byMIME(value);

    if (i == -1) {	/* was user's typo or not yet recognized value */
	i = LATIN1;	/* error recovery? */
	CTRACE(tfp, "safeUCGetLYhndl_byMIME: ISO-8859-1 assumed.\n");
    }

    return(i);
}
