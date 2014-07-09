/*
 * $LynxId: UCdomap.c,v 1.96 2014/02/04 01:29:44 tom Exp $
 *
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
#include <UCdomap.h>
#include <UCMap.h>
#include <UCAux.h>
#include <UCDefs.h>
#include <LYCharSets.h>
#include <LYStrings.h>
#include <LYUtils.h>

#if defined(USE_LOCALE_CHARSET) && defined(HAVE_LANGINFO_CODESET)
#include <langinfo.h>
#endif

#ifdef EXP_JAPANESEUTF8_SUPPORT
#include <iconv.h>
#endif

#include <LYLeaks.h>

/*
 * Include chartrans tables:
 */
#include <cp1250_uni.h>		/* WinLatin2 (cp1250)   */
#include <cp1251_uni.h>		/* WinCyrillic (cp1251) */
#include <cp1252_uni.h>		/* WinLatin1 (cp1252)   */
#include <cp1253_uni.h>		/* WinGreek (cp1253)    */
#include <cp1255_uni.h>		/* WinHebrew (cp1255)   */
#include <cp1256_uni.h>		/* WinArabic (cp1256)   */
#include <cp1257_uni.h>		/* WinBaltRim (cp1257)  */
#include <cp437_uni.h>		/* DosLatinUS (cp437)   */
#include <cp737_uni.h>		/* DosGreek (cp737)     */
#include <cp775_uni.h>		/* DosBaltRim (cp775)   */
#include <cp850_uni.h>		/* DosLatin1 (cp850)    */
#include <cp852_uni.h>		/* DosLatin2 (cp852)    */
#include <cp857_uni.h>		/* DosTurkish (cp857)   */
#include <cp862_uni.h>		/* DosHebrew (cp862)    */
#include <cp864_uni.h>		/* DosArabic (cp864)    */
#include <cp866_uni.h>		/* DosCyrillic (cp866)  */
#include <cp869_uni.h>		/* DosGreek2 (cp869)    */
#include <def7_uni.h>		/* 7 bit approximations */
#include <dmcs_uni.h>		/* DEC Multinational    */
#include <hp_uni.h>		/* HP Roman8            */
#include <iso01_uni.h>		/* ISO Latin 1          */
#include <iso02_uni.h>		/* ISO Latin 2          */
#include <iso03_uni.h>		/* ISO Latin 3          */
#include <iso04_uni.h>		/* ISO Latin 4          */
#include <iso05_uni.h>		/* ISO 8859-5 Cyrillic  */
#include <iso06_uni.h>		/* ISO 8859-6 Arabic    */
#include <iso07_uni.h>		/* ISO 8859-7 Greek     */
#include <iso08_uni.h>		/* ISO 8859-8 Hebrew    */
#include <iso09_uni.h>		/* ISO 8859-9 (Latin 5) */
#include <iso10_uni.h>		/* ISO 8859-10          */
#include <iso13_uni.h>		/* ISO 8859-13 (Latin 7) */
#include <iso14_uni.h>		/* ISO 8859-14 (Latin 8) */
#include <iso15_uni.h>		/* ISO 8859-15 (Latin 9) */
#include <koi8r_uni.h>		/* KOI8-R Cyrillic      */
#include <mac_uni.h>		/* Macintosh (8 bit)    */
#include <mnem2_suni.h>		/* RFC 1345 Mnemonic    */
#include <next_uni.h>		/* NeXT character set   */
#include <rfc_suni.h>		/* RFC 1345 w/o Intro   */
/* #include <utf8_uni.h> */ /* UNICODE UTF 8        */
#include <viscii_uni.h>		/* Vietnamese (VISCII)  */
#include <cp866u_uni.h>		/* Ukrainian Cyrillic (866) */
#include <koi8u_uni.h>		/* Ukrainian Cyrillic (koi8-u */
#include <pt154_uni.h>		/* Cyrillic-Asian (PT154) */

#ifdef CAN_AUTODETECT_DISPLAY_CHARSET
int auto_display_charset = -1;
#endif

static const char *UC_GNsetMIMEnames[4] =
{
    "iso-8859-1", "x-dec-graphics", "cp437", "x-transparent"
};

static int UC_GNhandles[4] =
{
    -1, -1, -1, -1
};

/*
 * Some of the code below, and some of the comments, are left in for
 * historical reasons.  Not all those tables below are currently
 * really needed (and what with all those hardwired codepoints),
 * but let's keep them around for now.  They may come in handy if we
 * decide to make more extended use of the mechanisms (including e.g.
 * for chars < 127...).  - KW
 */

static u16 translations[][256] =
{
    /*
     * 8-bit Latin-1 mapped to Unicode -- trivial mapping.
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
     * VT100 graphics mapped to Unicode.
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
     * IBM Codepage 437 mapped to Unicode.
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
     * User mapping -- default to codes for direct font mapping.
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
static u16 *UC_translate = NULL;

static struct UC_charset UCInfo[MAXCHARSETS];

/*
 * The standard kernel character-to-font mappings are not invertible
 * -- this is just a best effort.
 */
#define MAX_GLYPH 512		/* Max possible glyph value */

static unsigned char *inv_translate = NULL;
static unsigned char inv_norm_transl[MAX_GLYPH];
static unsigned char *inverse_translations[4] =
{NULL, NULL, NULL, NULL};

static void set_inverse_transl(int i);
static u16 *set_translate(int m);
static int UC_valid_UC_charset(int UC_charset_hndl);
static void UC_con_set_trans(int UC_charset_in_hndl, int Gn, int update_flag);
static int con_insert_unipair(unsigned unicode, unsigned fontpos, int fordefault);
static int con_insert_unipair_str(unsigned unicode, const char *replace_str, int fordefault);
static void con_clear_unimap(int fordefault);
static void con_clear_unimap_str(int fordefault);
static void con_set_default_unimap(void);
static int UC_con_set_unimap(int UC_charset_out_hndl, int update_flag);
static int UC_con_set_unimap_str(unsigned ct, struct unipair_str *list, int fordefault);
static int conv_uni_to_pc(long ucs, int usedefault);
static int conv_uni_to_str(char *outbuf, int buflen, UCode_t ucs, int usedefault);
static void UCconsole_map_init(void);
static int UC_MapGN(int UChndl, int update_flag);
static int UC_FindGN_byMIME(const char *UC_MIMEcharset);
static void UCreset_allocated_LYCharSets(void);
static STRING2PTR UC_setup_LYCharSets_repl(int UC_charset_in_hndl, unsigned lowest8);
static int UC_Register_with_LYCharSets(int s,
				       const char *UC_MIMEcharset,
				       const char *UC_LYNXcharset,
				       int lowest_eightbit);

#ifdef LY_FIND_LEAKS
static void UCfree_allocated_LYCharSets(void);
static void UCcleanup_mem(void);
#endif

static int default_UChndl = -1;

static void set_inverse_transl(int i)
{
    int j, glyph;
    u16 *p = translations[i];
    unsigned char *q = inverse_translations[i];

    if (!q) {
	/*
	 * Slightly messy to avoid calling kmalloc too early.
	 */
	q = inverse_translations[i] = ((i == LAT1_MAP) ?
				       inv_norm_transl :
				       typeMallocn(unsigned char, MAX_GLYPH));

	if (!q)
	    return;
    }
    for (j = 0; j < MAX_GLYPH; j++)
	q[j] = 0;

    for (j = 0; j < E_TABSZ; j++) {
	glyph = conv_uni_to_pc((long) p[j], 0);
	if (glyph >= 0 && glyph < MAX_GLYPH && q[glyph] < 32) {
	    /*
	     * Prefer '-' above SHY etc.
	     */
	    q[glyph] = UCH(j);
	}
    }
}

static u16 *set_translate(int m)
{
    if (!inverse_translations[m])
	set_inverse_transl(m);
    inv_translate = inverse_translations[m];
    return translations[m];
}

static int UC_valid_UC_charset(int UC_charset_hndl)
{
    return (UC_charset_hndl >= 0 && UC_charset_hndl < UCNumCharsets);
}

static void UC_con_set_trans(int UC_charset_in_hndl,
			     int Gn,
			     int update_flag)
{
    int i, j;
    const u16 *p;
    u16 *ptrans;

    if (!UC_valid_UC_charset(UC_charset_in_hndl)) {
	CTRACE((tfp, "UC_con_set_trans: Invalid charset handle %d.\n",
		UC_charset_in_hndl));
	return;
    }
    ptrans = translations[Gn];
    p = UCInfo[UC_charset_in_hndl].unitable;
#if(0)
    if (p == UC_current_unitable) {	/* test whether pointers are equal */
	return;			/* nothing to be done */
    }
    /*
     * The font is always 256 characters - so far.
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
	set_inverse_transl(Gn);	/* Update inverse translation for this one */
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
static int hashtable_contents_valid = 0;	/* Use ASCII-only mode for bootup */
static int hashtable_str_contents_valid = 0;

static u16 **uni_pagedir[32] =
{
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

static char ***uni_pagedir_str[32] =
{
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

static const u16 *UC_current_unitable = NULL;
static struct unimapdesc_str *UC_current_unitable_str = NULL;

/*
 * Keep a second set of structures for the translation designated
 * as "default" - kw
 */
static int unidefault_contents_valid = 0;	/* Use ASCII-only mode for bootup */
static int unidefault_str_contents_valid = 0;

static u16 **unidefault_pagedir[32] =
{
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};
static char ***unidefault_pagedir_str[32] =
{
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

static const u16 *UC_default_unitable = 0;
static const struct unimapdesc_str *UC_default_unitable_str = 0;

static int con_insert_unipair(unsigned unicode, unsigned fontpos, int fordefault)
{
    int i;
    unsigned n;
    u16 **p1, *p2;

    if (fordefault)
	p1 = unidefault_pagedir[n = unicode >> 11];
    else
	p1 = uni_pagedir[n = unicode >> 11];
    if (!p1) {
	p1 = (u16 * *)malloc(32 * sizeof(u16 *));
	if (fordefault)
	    unidefault_pagedir[n] = p1;
	else
	    uni_pagedir[n] = p1;
	if (!p1)
	    return ucError;

	for (i = 0; i < 32; i++) {
	    p1[i] = NULL;
	}
    }

    if (!(p2 = p1[n = (unicode >> 6) & 0x1f])) {
	p2 = p1[n] = (u16 *) malloc(64 * sizeof(u16));
	if (!p2)
	    return ucError;

	for (i = 0; i < 64; i++) {
	    p2[i] = 0xffff;	/* No glyph for this character (yet) */
	}
    }

    p2[unicode & 0x3f] = (u16) fontpos;

    return 0;
}

static int con_insert_unipair_str(unsigned unicode, const char *replace_str,
				  int fordefault)
{
    int i;
    unsigned n;
    char ***p1;
    const char **p2;

    if (fordefault)
	p1 = unidefault_pagedir_str[n = unicode >> 11];
    else
	p1 = uni_pagedir_str[n = unicode >> 11];
    if (!p1) {
	p1 = (char ***) malloc(32 * sizeof(char **));

	if (fordefault)
	    unidefault_pagedir_str[n] = p1;
	else
	    uni_pagedir_str[n] = p1;
	if (!p1)
	    return ucError;

	for (i = 0; i < 32; i++) {
	    p1[i] = NULL;
	}
    }

    n = ((unicode >> 6) & 0x1f);
    if (!p1[n]) {
	p1[n] = (char **) malloc(64 * sizeof(char *));

	if (!p1[n])
	    return ucError;

	p2 = (const char **) p1[n];
	for (i = 0; i < 64; i++) {
	    p2[i] = NULL;	/* No replace string this character (yet) */
	}
    }
    p2 = (const char **) p1[n];

    p2[unicode & 0x3f] = replace_str;

    return 0;
}

/*
 * ui arg was a leftover, deleted.  - KW
 */
static void con_clear_unimap(int fordefault)
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

static void con_clear_unimap_str(int fordefault)
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

	hashtable_str_contents_valid = 1;	/* ??? probably no use... */
    }
}

/*
 * Loads the unimap for the hardware font, as defined in uni_hash.tbl.
 * The representation used was the most compact I could come up
 * with.  This routine is executed at sys_setup time, and when the
 * PIO_FONTRESET ioctl is called.
 */
static void con_set_default_unimap(void)
{
    int i, j;
    const u16 *p;

    /*
     * The default font is always 256 characters.
     */
    con_clear_unimap(1);

    p = dfont_unitable;
    for (i = 0; i < 256; i++) {
	for (j = dfont_unicount[i]; j; j--) {
	    con_insert_unipair(*(p++), (u16) i, 1);
	}
    }

    UC_default_unitable = dfont_unitable;

    con_clear_unimap_str(1);
    UC_con_set_unimap_str(dfont_replacedesc.entry_ct, repl_map, 1);
    UC_default_unitable_str = &dfont_replacedesc;
}

int UCNumCharsets = 0;

int UCLYhndl_HTFile_for_unspec = -1;
int UCLYhndl_HTFile_for_unrec = -1;
int UCLYhndl_for_unspec = -1;
int UCLYhndl_for_unrec = -1;

/* easy to type, will initialize later */
int LATIN1 = -1;		/* UCGetLYhndl_byMIME("iso-8859-1") */
int US_ASCII = -1;		/* UCGetLYhndl_byMIME("us-ascii")   */
int UTF8_handle = -1;		/* UCGetLYhndl_byMIME("utf-8")      */
int TRANSPARENT = -1;		/* UCGetLYhndl_byMIME("x-transparent")  */

static int UC_con_set_unimap(int UC_charset_out_hndl,
			     int update_flag)
{
    int i, j;
    const u16 *p;

    if (!UC_valid_UC_charset(UC_charset_out_hndl)) {
	CTRACE((tfp, "UC_con_set_unimap: Invalid charset handle %d.\n",
		UC_charset_out_hndl));
	return ucError;
    }

    p = UCInfo[UC_charset_out_hndl].unitable;
    if (p == UC_current_unitable) {	/* test whether pointers are equal */
	return update_flag;	/* nothing to be done */
    }
    UC_current_unitable = p;

    /*
     * The font is always 256 characters - so far.
     */
    con_clear_unimap(0);

    for (i = 0; i < 256; i++) {
	for (j = UCInfo[UC_charset_out_hndl].unicount[i]; j; j--) {
	    con_insert_unipair(*(p++), (u16) i, 0);
	}
    }

    if (update_flag) {
	for (i = 0; i <= 3; i++) {
	    set_inverse_transl(i);	/* Update all inverse translations */
	}
    }

    return 0;
}

static int UC_con_set_unimap_str(unsigned ct, struct unipair_str *list,
				 int fordefault)
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
     * No inverse translations for replacement strings!
     */
    if (!err) {
	if (fordefault)
	    unidefault_str_contents_valid = 1;
	else
	    hashtable_str_contents_valid = 1;
    }

    return err;
}

static int conv_uni_to_pc(long ucs,
			  int usedefault)
{
    int h;
    u16 **p1, *p2;

    /*
     * Only 16-bit codes supported at this time.
     */
    if (ucs > 0xffff) {
	/*
	 * U+FFFD:  REPLACEMENT CHARACTER.
	 */
	ucs = 0xfffd;
    } else if (ucs < 0x20 || ucs >= 0xfffe) {
	/*
	 * Not a printable character.
	 */
	return ucError;
    } else if (ucs == 0xfeff || (ucs >= 0x200b && ucs <= 0x200f)) {
	/*
	 * Zero-width space.
	 */
	return ucZeroWidth;
    } else if ((ucs & ~UNI_DIRECT_MASK) == UNI_DIRECT_BASE) {
	/*
	 * UNI_DIRECT_BASE indicates the start of the region in the
	 * User Zone which always has a 1:1 mapping to the currently
	 * loaded font.  The UNI_DIRECT_MASK indicates the bit span
	 * of the region.
	 */
	return (ucs & UNI_DIRECT_MASK);
    }

    if (usedefault) {
	if (!unidefault_contents_valid)
	    return ucInvalidHash;
	p1 = unidefault_pagedir[ucs >> 11];
    } else {
	if (!hashtable_contents_valid)
	    return ucInvalidHash;
	p1 = uni_pagedir[ucs >> 11];
    }

    if (p1 &&
	(p2 = p1[(ucs >> 6) & 0x1f]) &&
	(h = p2[ucs & 0x3f]) < MAX_GLYPH) {
	return h;
    }

    /*
     * Not found.
     */
    return ucNotFound;
}

/*
 * Note:  contents of outbuf is not changes for negative return value!
 */
static int conv_uni_to_str(char *outbuf,
			   int buflen,
			   UCode_t ucs,
			   int usedefault)
{
    char *h;
    char ***p1, **p2;

    /*
     * Only 16-bit codes supported at this time.
     */
    if (ucs > 0xffff) {
	/*
	 * U+FFFD:  REPLACEMENT CHARACTER.
	 */
	ucs = 0xfffd;
	/*
	 * Maybe the following two cases should be allowed here??  - KW
	 */
    } else if (ucs < 0x20 || ucs >= 0xfffe) {
	/*
	 * Not a printable character.
	 */
	return ucError;
    } else if (ucs == 0xfeff || (ucs >= 0x200b && ucs <= 0x200f)) {
	/*
	 * Zero-width space.
	 */
	return ucZeroWidth;
    }

    if (usedefault) {
	if (!unidefault_str_contents_valid)
	    return ucInvalidHash;
	p1 = unidefault_pagedir_str[ucs >> 11];
    } else {
	if (!hashtable_str_contents_valid)
	    return ucInvalidHash;
	p1 = uni_pagedir_str[ucs >> 11];
    }

    if (p1 &&
	(p2 = p1[(ucs >> 6) & 0x1f]) &&
	(h = p2[ucs & 0x3f])) {
	StrNCpy(outbuf, h, (buflen - 1));
	return 1;		/* ok ! */
    }

    /*
     * Not found.
     */
    return ucNotFound;
}

int UCInitialized = 0;

/*
 * [ original comment:  - KW ]
 * This is called at sys_setup time, after memory and the console are
 * initialized.  It must be possible to call kmalloc(..., GFP_KERNEL)
 * from this function, hence the call from sys_setup.
 */
static void UCconsole_map_init(void)
{
    con_set_default_unimap();
    UCInitialized = 1;
}

/*
 * OK now, finally, some stuff that is more specifically for Lynx:  - KW
 */
int UCTransUniChar(UCode_t unicode,
		   int charset_out)
{
    int rc = 0;
    int UChndl_out;
    int isdefault, trydefault = 0;
    const u16 *ut;

    if ((UChndl_out = LYCharSet_UC[charset_out].UChndl) < 0) {
	if (LYCharSet_UC[charset_out].codepage < 0) {
	    if (unicode < 128) {
		rc = (int) unicode;
	    } else {
		rc = LYCharSet_UC[charset_out].codepage;
	    }
	    return rc;
	}
	if ((UChndl_out = default_UChndl) < 0) {
	    return ucCannotOutput;
	}
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
	if (rc >= 0) {
	    return rc;
	}
    }
    if (isdefault || trydefault) {
	rc = conv_uni_to_pc(unicode, 1);
	if (rc >= 0) {
	    return rc;
	}
    }
    if (!isdefault && (rc == ucNotFound)) {
	rc = conv_uni_to_pc(0xfffdL, 0);
    }
    if ((isdefault || trydefault) && (rc == ucNotFound)) {
	rc = conv_uni_to_pc(0xfffdL, 1);
    }
    return rc;
}

/*
 * Returns string length, or negative value for error.
 */
int UCTransUniCharStr(char *outbuf,
		      int buflen,
		      UCode_t unicode,
		      int charset_out,
		      int chk_single_flag)
{
    int rc = ucUnknown, src = 0;
    int UChndl_out;
    int isdefault, trydefault = 0;
    struct unimapdesc_str *repl;
    const u16 *ut;

    if (buflen < 2)
	return ucBufferTooSmall;

    if ((UChndl_out = LYCharSet_UC[charset_out].UChndl) < 0) {
	if (LYCharSet_UC[charset_out].codepage < 0)
	    return LYCharSet_UC[charset_out].codepage;
	if ((UChndl_out = default_UChndl) < 0)
	    return ucCannotOutput;
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
	    outbuf[0] = (char) src;
	    outbuf[1] = '\0';
	    return 1;
	}
    }

    repl = &(UCInfo[UChndl_out].replacedesc);
    if (!isdefault) {
	if (repl != UC_current_unitable_str) {
	    con_clear_unimap_str(0);
	    (void) UC_con_set_unimap_str(repl->entry_ct, repl->entries, 0);
	    UC_current_unitable_str = repl;
	}
	rc = conv_uni_to_str(outbuf, buflen, unicode, 0);
	if (rc >= 0)
	    return (int) strlen(outbuf);
    }
    if (trydefault && chk_single_flag) {
	src = conv_uni_to_pc(unicode, 1);
	if (src >= 32) {
	    outbuf[0] = (char) src;
	    outbuf[1] = '\0';
	    return 1;
	}
    }
    if (isdefault || trydefault) {
#ifdef EXP_JAPANESEUTF8_SUPPORT
	if (LYCharSet_UC[charset_out].codepage == 0 &&
	    LYCharSet_UC[charset_out].codepoints == 0) {
	    iconv_t cd;
	    char str[3], *pin, *pout;
	    size_t inleft, outleft;
	    char *tocode = NULL;

	    str[0] = (char) (unicode >> 8);
	    str[1] = (char) (unicode & 0xFF);
	    str[2] = 0;
	    pin = str;
	    inleft = 2;
	    pout = outbuf;
	    outleft = (size_t) buflen;
	    /*
	     * Try TRANSLIT first, since it is an extension which can provide
	     * translations when there is no available exact translation to
	     * the target character set.
	     */
	    HTSprintf0(&tocode, "%s//TRANSLIT", LYCharSet_UC[charset_out].MIMEname);
	    cd = iconv_open(tocode, "UTF-16BE");
	    if (cd == (iconv_t) -1) {
		/*
		 * Try again, without TRANSLIT
		 */
		HTSprintf0(&tocode, "%s", LYCharSet_UC[charset_out].MIMEname);
		cd = iconv_open(tocode, "UTF-16BE");

		if (cd == (iconv_t) -1) {
		    CTRACE((tfp,
			    "Warning: Cannot transcode form charset %s to %s!\n",
			    "UTF-16BE", tocode));
		}
	    }
	    FREE(tocode);

	    if (cd != (iconv_t) -1) {
		rc = (int) iconv(cd, (ICONV_CONST char **) &pin, &inleft,
				 &pout, &outleft);
		iconv_close(cd);
		if ((pout - outbuf) == 3) {
		    CTRACE((tfp,
			    "It seems to be a JIS X 0201 code(%" PRI_UCode_t
			    "). Not supported.\n", unicode));
		    pin = str;
		    inleft = 2;
		    pout = outbuf;
		    outleft = (size_t) buflen;
		} else if (rc >= 0) {
		    *pout = '\0';
		    return (int) strlen(outbuf);
		}
	    }
	}
#endif
	rc = conv_uni_to_str(outbuf, buflen, unicode, 1);
	if (rc >= 0)
	    return (int) strlen(outbuf);
    }
    if (rc == ucNotFound) {
	if (!isdefault)
	    rc = conv_uni_to_str(outbuf, buflen, 0xfffdL, 0);
	if ((rc == ucNotFound) && (isdefault || trydefault))
	    rc = conv_uni_to_str(outbuf, buflen, 0xfffdL, 1);
	if (rc >= 0)
	    return (int) strlen(outbuf);
    }
    if (chk_single_flag && src == ucNotFound) {
	if (!isdefault)
	    rc = conv_uni_to_pc(0xfffdL, 0);
	if ((rc == ucNotFound) && (isdefault || trydefault))
	    rc = conv_uni_to_pc(0xfffdL, 1);
	if (rc >= 32) {
	    outbuf[0] = (char) rc;
	    outbuf[1] = '\0';
	    return 1;
	}
	return rc;
    }
    return ucNotFound;
}

static int UC_lastautoGN = 0;

static int UC_MapGN(int UChndl,
		    int update_flag)
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
    CTRACE((tfp, "UC_MapGN: Using %d <- %d (%s)\n",
	    Gn, UChndl, UCInfo[UChndl].MIMEname));
    UC_con_set_trans(UChndl, Gn, update_flag);
    return Gn;
}

int UCTransChar(int ch_in,
		int charset_in,
		int charset_out)
{
    UCode_t unicode;
    int Gn;
    int rc = ucNotFound;
    int UChndl_in, UChndl_out;
    int isdefault, trydefault = 0;
    const u16 *ut;
    int upd = 0;

    if (charset_in == charset_out)
	return UCH(ch_in);
    if (charset_in < 0)
	return ucCannotConvert;
    if ((UChndl_in = LYCharSet_UC[charset_in].UChndl) < 0)
	return ucCannotConvert;
    if ((UChndl_out = LYCharSet_UC[charset_out].UChndl) < 0) {
	if (LYCharSet_UC[charset_out].codepage < 0)
	    return LYCharSet_UC[charset_out].codepage;
	if ((UChndl_out = default_UChndl) < 0)
	    return ucCannotOutput;
	isdefault = 1;
    } else {
	isdefault = UCInfo[UChndl_out].replacedesc.isdefault;
	trydefault = UCInfo[UChndl_out].replacedesc.trydefault;
    }
    if (!UCInfo[UChndl_in].num_uni)
	return ucCannotConvert;
    if ((Gn = UCInfo[UChndl_in].GN) < 0) {
	Gn = UC_MapGN(UChndl_in, 0);
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
    unicode = UC_translate[UCH(ch_in)];
    if (!isdefault) {
	rc = conv_uni_to_pc(unicode, 0);
	if (rc >= 0)
	    return rc;
    }
    if ((rc == ucNotFound) && (isdefault || trydefault)) {
	rc = conv_uni_to_pc(unicode, 1);
    }
    if ((rc == ucNotFound) && !isdefault) {
	rc = conv_uni_to_pc(0xfffdL, 0);
    }
    if ((rc == ucNotFound) && (isdefault || trydefault)) {
	rc = conv_uni_to_pc(0xfffdL, 1);
    }
    return rc;
}

#ifdef EXP_JAPANESEUTF8_SUPPORT
UCode_t UCTransJPToUni(char *inbuf,
		       int buflen,
		       int charset_in)
{
    char outbuf[3], *pin, *pout;
    size_t ilen, olen;
    iconv_t cd;

    pin = inbuf;
    pout = outbuf;
    ilen = 2;
    olen = (size_t) buflen;

    cd = iconv_open("UTF-16BE", LYCharSet_UC[charset_in].MIMEname);
    (void) iconv(cd, (ICONV_CONST char **) &pin, &ilen, &pout, &olen);
    iconv_close(cd);
    if ((ilen == 0) && (olen == 0)) {
	return (((unsigned char) outbuf[0]) << 8) + (unsigned char) outbuf[1];
    }
    return ucCannotConvert;
}
#endif

/*
 * Translate a character to Unicode.  If additional bytes are needed, this
 * returns ucNeedMore, based on its internal state.  To reset the state,
 * call this with charset_in < 0.
 */
UCode_t UCTransToUni(int ch_in,
		     int charset_in)
{
    static char buffer[10];
    static unsigned inx = 0;

    UCode_t unicode;
    int Gn;
    unsigned char ch_iu = UCH(ch_in);
    int UChndl_in;

    /*
     * Reset saved-state.
     */
    if (charset_in < 0) {
	inx = 0;
	return ucCannotConvert;
    } else if (charset_in == LATIN1) {
	return ch_iu;
    } else if (charset_in == UTF8_handle) {
	if (is8bits(ch_iu)) {
	    unsigned need;
	    char *ptr;

	    buffer[inx++] = (char) ch_iu;
	    buffer[inx] = '\0';
	    need = (unsigned) utf8_length(TRUE, buffer);
	    if (need && (need + 1) == inx) {
		inx = 0;
		ptr = buffer;
		return UCGetUniFromUtf8String(&ptr);
	    } else if (inx < sizeof(buffer) - 1) {
		return ucNeedMore;
	    } else {
		inx = 0;
	    }
	} else {
	    inx = 0;
	}
    }
#ifdef EXP_JAPANESEUTF8_SUPPORT
    if ((strcmp(LYCharSet_UC[charset_in].MIMEname, "shift_jis") == 0) ||
	(strcmp(LYCharSet_UC[charset_in].MIMEname, "euc-jp") == 0)) {
	char obuffer[3], *pin, *pout;
	size_t ilen, olen;
	iconv_t cd;

	pin = buffer;
	pout = obuffer;
	ilen = olen = 2;
	if (strcmp(LYCharSet_UC[charset_in].MIMEname, "shift_jis") == 0) {
	    if (inx == 0) {
		if (IS_SJIS_HI1(ch_iu) ||
		    IS_SJIS_HI2(ch_iu)) {
		    buffer[0] = (char) ch_in;
		    inx = 1;
		    return ucNeedMore;
		}
	    } else {
		if (IS_SJIS_LO(ch_iu)) {
		    buffer[1] = (char) ch_in;
		    buffer[2] = 0;

		    cd = iconv_open("UTF-16BE", "Shift_JIS");
		    (void) iconv(cd, (ICONV_CONST char **) &pin, &ilen, &pout, &olen);
		    iconv_close(cd);
		    inx = 0;
		    if ((ilen == 0) && (olen == 0)) {
			return (UCH(obuffer[0]) << 8) + UCH(obuffer[1]);
		    }
		}
	    }
	}
	if (strcmp(LYCharSet_UC[charset_in].MIMEname, "euc-jp") == 0) {
	    if (inx == 0) {
		if (IS_EUC_HI(ch_iu)) {
		    buffer[0] = (char) ch_in;
		    inx = 1;
		    return ucNeedMore;
		}
	    } else {
		if (IS_EUC_LOX(ch_iu)) {
		    buffer[1] = (char) ch_in;
		    buffer[2] = 0;

		    cd = iconv_open("UTF-16BE", "EUC-JP");
		    (void) iconv(cd, (ICONV_CONST char **) &pin, &ilen, &pout, &olen);
		    iconv_close(cd);
		    inx = 0;
		    if ((ilen == 0) && (olen == 0)) {
			return (UCH(obuffer[0]) << 8) + UCH(obuffer[1]);
		    }
		}
	    }
	}
	inx = 0;
    }
#endif
    if (ch_iu < 128 && ch_iu >= 32)
	return ch_iu;

    if (ch_iu < 32 &&
	LYCharSet_UC[charset_in].enc != UCT_ENC_8BIT_C0) {
	/*
	 * Don't translate C0 chars except for specific charsets.
	 */
	return ch_iu;
    } else if ((UChndl_in = LYCharSet_UC[charset_in].UChndl) < 0) {
	return ucCannotConvert;
    } else if (!UCInfo[UChndl_in].num_uni) {
	return ucCannotConvert;
    }

    if ((Gn = UCInfo[UChndl_in].GN) < 0) {
	Gn = UC_MapGN(UChndl_in, 1);
    }

    UC_translate = set_translate(Gn);
    unicode = UC_translate[ch_iu];

    return unicode;
}

int UCReverseTransChar(int ch_out,
		       int charset_in,
		       int charset_out)
{
    int Gn;
    int rc = ucError;
    int UChndl_in, UChndl_out;
    int isdefault;
    int i_ch = UCH(ch_out);
    const u16 *ut;

    if (charset_in == charset_out)
	return UCH(ch_out);
    if (charset_in < 0)
	return ucCannotConvert;
    if ((UChndl_in = LYCharSet_UC[charset_in].UChndl) < 0)
	return ucCannotConvert;
    if (!UCInfo[UChndl_in].num_uni)
	return ucCannotConvert;
    if (charset_out < 0)
	return ucCannotOutput;
    if ((UChndl_out = LYCharSet_UC[charset_out].UChndl) < 0) {
	if (LYCharSet_UC[charset_out].codepage < 0)
	    return LYCharSet_UC[charset_out].codepage;
	if ((UChndl_out = default_UChndl) < 0)
	    return ucCannotOutput;
	isdefault = 1;
    } else {
	isdefault = UCInfo[UChndl_out].replacedesc.isdefault;
    }

    if (!isdefault) {
	/*
	 * Try to use the inverse table if charset_out is not equivalent
	 * to using just the default table.  If it is, it should have
	 * just ASCII chars and trying to back-translate those should
	 * not give anything but themselves.  - kw
	 */
	ut = UCInfo[UChndl_out].unitable;
	if (ut == UC_current_unitable) {
	    if ((Gn = UCInfo[UChndl_in].GN) < 0) {
		Gn = UC_MapGN(UChndl_in, 1);
	    }
	    UC_translate = set_translate(Gn);
	    if (inv_translate)
		rc = inv_translate[i_ch];
	    if (rc >= 32) {
		return rc;
	    }
	}
    }
    return UCTransChar(ch_out, charset_out, charset_in);
}

/*
 * Returns string length, or negative value for error.
 */
int UCTransCharStr(char *outbuf,
		   int buflen,
		   int ch_in,
		   int charset_in,
		   int charset_out,
		   int chk_single_flag)
{
    UCode_t unicode;
    int Gn;
    int rc = ucUnknown, src = 0;
    int UChndl_in, UChndl_out;
    int isdefault, trydefault = 0;
    struct unimapdesc_str *repl;
    const u16 *ut;
    int upd = 0;

    if (buflen < 2)
	return ucBufferTooSmall;
    if (chk_single_flag && charset_in == charset_out) {
	outbuf[0] = (char) ch_in;
	outbuf[1] = '\0';
	return 1;
    }
    if (charset_in < 0)
	return ucCannotConvert;
    if ((UChndl_in = LYCharSet_UC[charset_in].UChndl) < 0)
	return ucCannotConvert;
    if (!UCInfo[UChndl_in].num_uni)
	return ucCannotConvert;
    if ((UChndl_out = LYCharSet_UC[charset_out].UChndl) < 0) {
	if (LYCharSet_UC[charset_out].codepage < 0)
	    return LYCharSet_UC[charset_out].codepage;
	if ((UChndl_out = default_UChndl) < 0)
	    return ucCannotOutput;
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
    unicode = UC_translate[UCH(ch_in)];

    if (chk_single_flag) {
	if (!isdefault) {
	    ut = UCInfo[UChndl_out].unitable;
	    if (ut == UC_current_unitable) {
		if (upd)
		    set_inverse_transl(Gn);
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
	    outbuf[0] = (char) src;
	    outbuf[1] = '\0';
	    return 1;
	}
    }

    repl = &(UCInfo[UChndl_out].replacedesc);
    if (!isdefault) {
	if (repl != UC_current_unitable_str) {
	    con_clear_unimap_str(0);
	    (void) UC_con_set_unimap_str(repl->entry_ct, repl->entries, 0);
	    UC_current_unitable_str = repl;
	}
	rc = conv_uni_to_str(outbuf, buflen, unicode, 0);
	if (rc >= 0)
	    return (int) strlen(outbuf);
    }
    if (trydefault && chk_single_flag) {
	src = conv_uni_to_pc(unicode, 1);
	if (src >= 32) {
	    outbuf[0] = (char) src;
	    outbuf[1] = '\0';
	    return 1;
	}
    }
    if (isdefault || trydefault) {
	rc = conv_uni_to_str(outbuf, buflen, unicode, 1);
	if (rc >= 0)
	    return (int) strlen(outbuf);
    }
    if (rc == ucNotFound) {
	if (!isdefault)
	    rc = conv_uni_to_str(outbuf, buflen, 0xfffdL, 0);
	if ((rc == ucNotFound) && (isdefault || trydefault))
	    rc = conv_uni_to_str(outbuf, buflen, 0xfffdL, 1);
	if (rc >= 0)
	    return (int) strlen(outbuf);
    }
    if (chk_single_flag && src == ucNotFound) {
	if (!isdefault)
	    rc = conv_uni_to_pc(0xfffdL, 0);
	if ((rc == ucNotFound) && (isdefault || trydefault))
	    rc = conv_uni_to_pc(0xfffdL, 1);
	if (rc >= 32) {
	    outbuf[0] = (char) rc;
	    outbuf[1] = '\0';
	    return 1;
	} else if (rc <= 0) {
	    outbuf[0] = '\0';
	    return rc;
	}
	return rc;
    }
    return ucNotFound;
}

static int UC_FindGN_byMIME(const char *UC_MIMEcharset)
{
    int i;

    for (i = 0; i < 4; i++) {
	if (!strcmp(UC_MIMEcharset, UC_GNsetMIMEnames[i])) {
	    return i;
	}
    }
    return ucError;
}

int UCGetRawUniMode_byLYhndl(int i)
{
    if (i < 0)
	return 0;
    return LYCharSet_UC[i].enc;
}

/*
 * Construct a new charset name, given prefix and codepage.  This introduces
 * potentially unchecked recursion into UCGetLYhntl_byMIME if neither the "cp"
 * nor "windows-" prefixes are configured, so we check it here.
 */
static int getLYhndl_byCP(const char *prefix,
			  const char *codepage)
{
    static int nested;
    int result = ucError;

    if (!nested++) {
	char *cptmp = NULL;

	StrAllocCopy(cptmp, prefix);
	StrAllocCat(cptmp, codepage);
	result = UCGetLYhndl_byMIME(cptmp);
	FREE(cptmp);
    }
    nested--;
    return result;
}

/*
 * Get Lynx internal charset handler from MIME name,
 * return -1 if we got NULL or did not recognize value.
 * According to RFC, MIME headers should match case-insensitively.
 */
int UCGetLYhndl_byMIME(const char *value)
{
    int i;
    int LYhndl = -1;

    if (isEmpty(value)) {
	CTRACE((tfp,
		"UCGetLYhndl_byMIME: NULL argument instead of MIME name.\n"));
	return ucError;
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
#if !NO_CHARSET_utf_8
    if (!strcasecomp(value, "unicode-1-1-utf-8") ||
	!strcasecomp(value, "utf8")) {
	/*
	 * Treat these as synonyms for the IANA registered name.  - FM
	 */
	return UCGetLYhndl_byMIME("utf-8");
    }
#endif
    if (!strncasecomp(value, "iso", 3) && !StrNCmp(value + 3, "8859", 4)) {
	return getLYhndl_byCP("iso-", value + 3);
    }
    if (!strcasecomp(value, "iso-8859-8-i") ||
	!strcasecomp(value, "iso-8859-8-e")) {
	return UCGetLYhndl_byMIME("iso-8859-8");
    }
#if !NO_CHARSET_euc_jp
    if (!strcasecomp(value, "x-euc-jp") ||
	!strcasecomp(value, "eucjp")) {
	return UCGetLYhndl_byMIME("euc-jp");
    }
#endif
#if !NO_CHARSET_shift_jis
    if ((!strcasecomp(value, "x-shift-jis")) ||
	(!strcasecomp(value, "x-sjis")) ||
	(!strcasecomp(value, "pck"))) {
	return UCGetLYhndl_byMIME("shift_jis");
    }
#endif
#if !NO_CHARSET_euc_kr
    if (!strcasecomp(value, "iso-2022-kr")) {
	return UCGetLYhndl_byMIME("euc-kr");
    }
#endif
#if !NO_CHARSET_euc_cn
    if (!strcasecomp(value, "gb2312") ||
	!strncasecomp(value, "cn-gb", 5) ||
	!strcasecomp(value, "iso-2022-cn")) {
	return UCGetLYhndl_byMIME("euc-cn");
    }
#endif
#if !NO_CHARSET_big5
    if (!strcasecomp(value, "cn-big5")) {
	return UCGetLYhndl_byMIME("big5");
    }
#endif
#if !NO_CHARSET_macintosh
    if (!strcasecomp(value, "x-mac-roman") ||
	!strcasecomp(value, "mac-roman")) {
	return UCGetLYhndl_byMIME("macintosh");
    }
#endif
#if !NO_CHARSET_next
    if (!strcasecomp(value, "x-next") ||
	!strcasecomp(value, "nextstep") ||
	!strcasecomp(value, "x-nextstep")) {
	return UCGetLYhndl_byMIME("next");
    }
#endif
#if !NO_CHARSET_windows_1252
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
#endif
#if !NO_CHARSET_windows_1251
    if (!strcasecomp(value, "ansi-1251")) {
	return UCGetLYhndl_byMIME("windows-1251");
    }
#endif
#if !NO_CHARSET_windows_1250
    if (!strcasecomp(value, "iso-8859-2-windows-latin-2") ||
	!strcasecomp(value, "cp1250") ||
	!strcasecomp(value, "cp-1250") ||
	!strcasecomp(value, "ibm1250")) {
	/*
	 * Treat these as synonyms for windows-1250.  - FM
	 */
	return UCGetLYhndl_byMIME("windows-1250");
    }
#endif
    if ((!strncasecomp(value, "ibm", 3) ||
	 !strncasecomp(value, "cp-", 3)) &&
	isdigit(UCH(value[3])) &&
	isdigit(UCH(value[4])) &&
	isdigit(UCH(value[5]))) {
	/*
	 * For "ibmNNN<...>" or "cp-NNN", try "cpNNN<...>"
	 * if not yet found.  - KW & FM
	 */
	if ((LYhndl = getLYhndl_byCP("cp", value + 3)) >= 0)
	    return LYhndl;
	/*
	 * Try windows-NNN<...> if not yet found.  - FM
	 */
	return getLYhndl_byCP("windows-", value + 3);
    }
    if (!strncasecomp(value, "windows-", 8) &&
	isdigit(UCH(value[8])) &&
	isdigit(UCH(value[9])) &&
	isdigit(UCH(value[10]))) {
	/*
	 * For "windows-NNN<...>", try "cpNNN<...>" - FM
	 */
	return getLYhndl_byCP("cp", value + 8);
    }
#if !NO_CHARSET_koi8_r
    if (!strcasecomp(value, "koi-8")) {		/* accentsoft bugosity */
	return UCGetLYhndl_byMIME("koi8-r");
    }
#endif
    if (!strcasecomp(value, "ANSI_X3.4-1968")) {
	return US_ASCII;
    }
    /* no more synonyms if come here... */

    CTRACE((tfp, "UCGetLYhndl_byMIME: unrecognized MIME name \"%s\"\n", value));
    return ucError;		/* returns -1 if no charset found by that MIME name */
}

/*
 * Function UC_setup_LYCharSets_repl() tries to set up a subtable in
 * LYCharSets[] appropriate for this new charset, for compatibility with the
 * "old method".  Maybe not nice (maybe not even necessary any more), but it
 * works (as far as it goes..).
 *
 * We try to be conservative and only allocate new memory for this if needed.
 * If not needed, just point to SevenBitApproximations[i].  [Could do the same
 * for ISO_Latin1[] if it's identical to that, but would make it even *more*
 * messy than it already is...] This the only function in this file that knows,
 * or cares, about the HTMLDTD or details of LYCharSets[] subtables (and
 * therefore somewhat violates the idea that this file should be independent of
 * those).  As in other places, we rely on ISO_Latin1 being the *first* table
 * in LYCharSets.  - KW
 */

/*
 * We need to remember which ones were allocated and which are static.
 */
static STRING2PTR remember_allocated_LYCharSets[MAXCHARSETS];

static void UCreset_allocated_LYCharSets(void)
{
    int i = 0;

    for (; i < MAXCHARSETS; i++) {
	remember_allocated_LYCharSets[i] = NULL;
    }
}

#ifdef LY_FIND_LEAKS
static void UCfree_allocated_LYCharSets(void)
{
    int i = 0;

    for (; i < MAXCHARSETS; i++) {
	if (remember_allocated_LYCharSets[i] != NULL) {
	    FREE(remember_allocated_LYCharSets[i]);
	}
    }
}
#endif

static STRING2PTR UC_setup_LYCharSets_repl(int UC_charset_in_hndl,
					   unsigned lowest8)
{
    STRING2PTR ISO_Latin1 = LYCharSets[0];
    const char **p;
    char **prepl;
    const u16 *pp;
    const char **tp;
    const char *s7;
    const char *s8;
    size_t i;
    int j, changed;
    u16 k;
    u8 *ti;

    /*
     * Create a temporary table for reverse lookup of latin1 codes:
     */
    tp = (const char **) malloc(96 * sizeof(char *));

    if (!tp)
	return NULL;
    for (i = 0; i < 96; i++)
	tp[i] = NULL;
    ti = (u8 *) malloc(96 * sizeof(u8));
    if (!ti) {
	FREE(tp);
	return NULL;
    }
    for (i = 0; i < 96; i++)
	ti[i] = 0;

    pp = UCInfo[UC_charset_in_hndl].unitable;

    /*
     * Determine if we have any mapping of a Unicode in the range 160-255
     * to an allowed code point > 0x80 in our new charset...
     * Store any mappings found in ti[].
     */
    if (UCInfo[UC_charset_in_hndl].num_uni > 0) {
	for (i = 0; i < 256; i++) {
	    if ((j = UCInfo[UC_charset_in_hndl].unicount[i])) {
		if ((k = *pp) >= 160 && k < 256 && i >= lowest8) {
		    ti[k - 160] = UCH(i);
		}
		for (; j; j--) {
		    pp++;
		}
	    }
	}
    } {
	u16 ct;
	struct unipair_str *list;

	/*
	 * Determine if we have any mapping of a Unicode in the range
	 * 160-255 to a replacement string for our new charset...
	 * Store any mappings found in tp[].
	 */
	ct = UCInfo[UC_charset_in_hndl].replacedesc.entry_ct;
	list = UCInfo[UC_charset_in_hndl].replacedesc.entries;
	while (ct--) {
	    if ((k = list->unicode) >= 160 && k < 256) {
		tp[k - 160] = list->replace_str;
	    }
	    list++;
	}
    }
    /*
     * Now allocate a new table compatible with LYCharSets[]
     * and with the HTMLDTD for entities.
     * We don't know yet whether we'll keep it around.
     */
    prepl = (char **) malloc(HTML_dtd.number_of_entities * sizeof(char *));

    if (!prepl) {
	FREE(tp);
	FREE(ti);
	return 0;
    }

    p = (const char **) prepl;
    changed = 0;
    for (i = 0; i < HTML_dtd.number_of_entities; i++, p++) {
	/*
	 * For each of those entities, we check what the "old method"
	 * ISO_Latin1[] mapping does with them.  If it is nothing we
	 * want to use, just point to the SevenBitApproximations[] string.
	 */
	s7 = SevenBitApproximations[i];
	s8 = ISO_Latin1[i];
	*p = s7;
	if (s8 && UCH(*s8) >= 160 && s8[1] == '\0') {
	    /*
	     * We have an entity that is mapped to
	     * one valid eightbit latin1 char.
	     */
	    if (ti[UCH(*s8) - 160] >= UCH(lowest8) &&
		!(UCH(s7[0]) == ti[UCH(*s8) - 160] &&
		  s7[1] == '\0')) {
		/*
		 * ...which in turn is mapped, by our "new method",
		 * to another valid eightbit char for this new
		 * charset:  either to itself...
		 */
		if (ti[UCH(*s8) - 160] == UCH(*s8)) {
		    *p = s8;
		} else {
		    /*
		     * make those 1-char strings
		     * into HTAtoms, so they will be cleaned up
		     * at exit...  all for the sake of preventing
		     * memory leaks, sigh.
		     */
		    static char dummy[2];	/* one char dummy string */

		    dummy[0] = (char) ti[UCH(*s8) - 160];
		    *p = HTAtom_name(HTAtom_for(dummy));
		}
		changed = 1;
	    } else if (tp[UCH(*s8) - 160] &&
		       strcmp(s7, tp[UCH(*s8) - 160])) {
		/*
		 * ...or which is mapped, by our "new method",
		 * to a replacement string for this new charset.
		 */
		*p = tp[UCH(*s8) - 160];
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
    return (STRING2PTR) prepl;
}

/*
 * "New method" meets "Old method" ...
 */
static int UC_Register_with_LYCharSets(int s,
				       const char *UC_MIMEcharset,
				       const char *UC_LYNXcharset,
				       int lowest_eightbit)
{
    int i, LYhndl, found;
    STRING2PTR repl;

    LYhndl = -1;
    if (LYNumCharsets == 0) {
	/*
	 * Initialize here; so whoever changes
	 * LYCharSets.c doesn't have to count...
	 */
	for (i = 0; (i < MAXCHARSETS) && LYchar_set_names[i]; i++) {
	    LYNumCharsets = i + 1;
	}
    }

    /*
     * Search by MIME name, (LYchar_set_names may differ...)
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
	    CTRACE((tfp,
		    "UC_Register_with_LYCharSets: Too many.  Ignoring %s/%s.",
		    UC_MIMEcharset, UC_LYNXcharset));
	    return ucError;
	}
	/*
	 * Add to LYCharSets.c lists.
	 */
	LYhndl = LYNumCharsets;
	LYNumCharsets++;
	LYlowest_eightbit[LYhndl] = 999;
	LYCharSets[LYhndl] = SevenBitApproximations;
	/*
	 * Hmm, try to be conservative here.
	 */
	LYchar_set_names[LYhndl] = UC_LYNXcharset;
	LYchar_set_names[LYhndl + 1] = NULL;
	/*
	 * Terminating NULL may be looked for by Lynx code.
	 */
    } else {
	found = 1;
    }
    LYCharSet_UC[LYhndl].UChndl = s;
    /*
     * Can we just copy the pointer?  Hope so...
     */
    LYCharSet_UC[LYhndl].MIMEname = UC_MIMEcharset;
    LYCharSet_UC[LYhndl].enc = UCInfo[s].enc;
    LYCharSet_UC[LYhndl].codepage = UCInfo[s].codepage;

    /*
     * @@@ We really SHOULD get more info from the table files,
     * and set relevant flags in the LYCharSet_UC[] entry with
     * that info...  For now, let's try it without.  - KW
     */
    if (lowest_eightbit < LYlowest_eightbit[LYhndl]) {
	LYlowest_eightbit[LYhndl] = lowest_eightbit;
    } else if (lowest_eightbit > LYlowest_eightbit[LYhndl]) {
	UCInfo[s].lowest_eight = LYlowest_eightbit[LYhndl];
    }

    if (!found && LYhndl > 0) {
	repl = UC_setup_LYCharSets_repl(s, (unsigned) UCInfo[s].lowest_eight);
	if (repl) {
	    LYCharSets[LYhndl] = repl;
	    /*
	     * Remember to FREE at exit.
	     */
	    remember_allocated_LYCharSets[LYhndl] = repl;
	}
    }
    return LYhndl;
}

/*
 * This only sets up the structure - no initialization of the tables
 * is done here yet.
 */
void UC_Charset_Setup(const char *UC_MIMEcharset,
		      const char *UC_LYNXcharset,
		      const u8 * unicount,
		      const u16 * unitable,
		      int nnuni,
		      struct unimapdesc_str replacedesc,
		      int lowest_eight,
		      int UC_rawuni,
		      int codepage)
{
    int s, Gn;
    int i, status = 0, found;

    /*
     * Get (new?) slot.
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
	    CTRACE((tfp, "UC_Charset_Setup: Too many.  Ignoring %s/%s.",
		    UC_MIMEcharset, UC_LYNXcharset));
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
	lowest_eight = 128;	/* cheat here */
    UCInfo[s].lowest_eight = lowest_eight;
    UCInfo[s].enc = UC_rawuni;
    UCInfo[s].codepage = codepage;
    UCInfo[s].LYhndl = UC_Register_with_LYCharSets(s,
						   UC_MIMEcharset,
						   UC_LYNXcharset,
						   lowest_eight);
    CTRACE2(TRACE_CFG, (tfp, "registered charset %d mime \"%s\" lynx \"%s\"\n",
			s, UC_MIMEcharset, UC_LYNXcharset));
    UCInfo[s].uc_status = status;
    if (found < 0)
	UCNumCharsets++;
    return;
}

/*
 * UC_NoUctb_Register_with_LYCharSets, UC_Charset_NoUctb_Setup -
 * Alternative functions for adding character set info to the lists
 * kept in LYCharSets.c.
 *
 * These are for character sets without any real tables of their own.
 * We don't keep an entry in UCinfo[] for them.
 */
static int UC_NoUctb_Register_with_LYCharSets(const char *UC_MIMEcharset,
					      const char *UC_LYNXcharset,
					      int lowest_eightbit,
					      int UC_rawuni,
					      int codepage)
{
    int i, LYhndl = -1;

    if (LYNumCharsets == 0) {
	/*
	 * Initialize here; so whoever changes
	 * LYCharSets.c doesn't have to count...
	 */
	for (i = 0; (i < MAXCHARSETS) && LYchar_set_names[i]; i++) {
	    LYNumCharsets = i + 1;
	}
    }

    /*
     * Search by MIME name, (LYchar_set_names may differ...)
     * ignore if already present!
     */
    for (i = 0; i < MAXCHARSETS && LYchar_set_names[i] && LYhndl < 0; i++) {
	if (LYCharSet_UC[i].MIMEname &&
	    !strcmp(UC_MIMEcharset, LYCharSet_UC[i].MIMEname)) {
	    return ucError;
	}
    }

    /* not found */
    if (LYNumCharsets >= MAXCHARSETS) {
	CTRACE((tfp,
		"UC_NoUctb_Register_with_LYCharSets: Too many.  Ignoring %s/%s.",
		UC_MIMEcharset, UC_LYNXcharset));
	return ucError;
    }
    /*
     * Add to LYCharSets.c lists.
     */
    LYhndl = LYNumCharsets;
    LYNumCharsets++;
    LYlowest_eightbit[LYhndl] = lowest_eightbit;
    LYCharSets[LYhndl] = SevenBitApproximations;
    LYchar_set_names[LYhndl] = UC_LYNXcharset;
    LYchar_set_names[LYhndl + 1] = NULL;
    /*
     * Terminating NULL may be looked for by Lynx code.
     */

    LYCharSet_UC[LYhndl].UChndl = -1;	/* no corresponding UChndl ! */
    LYCharSet_UC[LYhndl].MIMEname = UC_MIMEcharset;
    LYCharSet_UC[LYhndl].enc = UC_rawuni;
    LYCharSet_UC[LYhndl].codepage = codepage;

    /*
     * @@@ We really SHOULD get more info from the table files,
     * and set relevant flags in the LYCharSet_UC[] entry with
     * that info...  For now, let's try it without.  - KW
     */

    return LYhndl;
}

/*
 * A wrapper for the previous function.
 */
static void UC_Charset_NoUctb_Setup(const char *UC_MIMEcharset,
				    const char *UC_LYNXcharset,
				    int trydefault,
				    int lowest_eight,
				    int UC_rawuni,
				    int codepage)
{
    int i;

    /*
     * Ignore completely if already in slot.
     */
    for (i = 0; i < UCNumCharsets; i++) {
	if (!strcmp(UCInfo[i].MIMEname, UC_MIMEcharset)) {
	    return;
	}
    }
    if (UC_rawuni == UCT_ENC_UTF8)
	lowest_eight = 128;	/* cheat here */
    /* 'codepage' doubles as a flag for 'do not try any table
     * lookup, not even default' when negative.  The value will
     * be returned immediately by UCTrans* functions.
     */
    if (!trydefault && codepage == 0)
	codepage = ucCannotOutput;	/* if not already set; any negative should do. */
    UC_NoUctb_Register_with_LYCharSets(UC_MIMEcharset,
				       UC_LYNXcharset,
				       lowest_eight,
				       UC_rawuni,
				       codepage);
    return;
}

#ifdef LY_FIND_LEAKS
static void UCcleanup_mem(void)
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

#ifdef EXP_CHARTRANS_AUTOSWITCH
#ifdef CAN_AUTODETECT_DISPLAY_CHARSET
#  ifdef __EMX__
static int CpOrdinal(const unsigned UCode_t cp, const int other)
{
    char lyName[80];
    char myMimeName[80];
    char *mimeName, *mName = NULL, *lName = NULL;
    int s, i, exists = 0, ret;

    CTRACE((tfp, "CpOrdinal(cp=%lu, other=%d).\n", cp, other));
    sprintf(myMimeName, "auto%s-cp%lu", (other ? "2" : ""), cp);
    mimeName = myMimeName + 5 + (other != 0);
    sprintf(lyName, "AutoDetect%s (cp%lu)",
	    (other ? "-2" : ""), cp);
    /* Find slot. */
    s = -1;
    for (i = 0; i < UCNumCharsets; i++) {
	if (!strcmp(UCInfo[i].LYNXname, lyName))
	    return UCGetLYhndl_byMIME(myMimeName);
	else if (!strcasecomp(UCInfo[i].MIMEname, mimeName))
	    s = i;
    }
    if (s < 0)
	return ucError;
    /* Store the "real" charset info */
    real_charsets[other != 0] = UCGetLYhndl_byMIME(mimeName);
    /* Duplicate the record. */
    StrAllocCopy(mName, myMimeName);
    StrAllocCopy(lName, lyName);
    UC_Charset_Setup(mName, lName,
		     UCInfo[s].unicount, UCInfo[s].unitable,
		     UCInfo[s].num_uni, UCInfo[s].replacedesc,
		     UCInfo[s].lowest_eight, UCInfo[s].enc,
		     UCInfo[s].codepage);
    ret = UCGetLYhndl_byMIME(myMimeName);
    CTRACE((tfp, "Found %i.\n", ret));
    return ret;
}
#  endif /* __EMX__ */
#endif /* CAN_AUTODETECT_DISPLAY_CHARSET */
#endif /* EXP_CHARTRANS_AUTOSWITCH */

void UCInit(void)
{

    UCreset_allocated_LYCharSets();
#ifdef LY_FIND_LEAKS
    atexit(UCcleanup_mem);
#endif
    UCconsole_map_init();

    /*
     * The order of charset names visible in Lynx Options menu correspond to
     * the order of lines below, except the first two described in LYCharSet.c
     *
     * Entries whose comment is marked with *** are declared in UCdomap.h,
     * others are based on the included tables - UCdomap.c, near the top.
     */

    UC_CHARSET_SETUP_iso_8859_1;	/* ISO Latin 1          */
    UC_CHARSET_SETUP_iso_8859_15;	/* ISO 8859-15 (Latin 9) */
    UC_CHARSET_SETUP_cp850;	/* DosLatin1 (cp850)    */
    UC_CHARSET_SETUP_windows_1252;	/* WinLatin1 (cp1252)   */
    UC_CHARSET_SETUP_cp437;	/* DosLatinUS (cp437)   */

    UC_CHARSET_SETUP_dec_mcs;	/* DEC Multinational    */
    UC_CHARSET_SETUP_macintosh;	/* Macintosh (8 bit)    */
    UC_CHARSET_SETUP_next;	/* NeXT character set   */
    UC_CHARSET_SETUP_hp_roman8;	/* HP Roman8            */

    UC_CHARSET_SETUP_euc_cn;		  /*** Chinese		    */
    UC_CHARSET_SETUP_euc_jp;		  /*** Japanese (EUC_JP)    */
    UC_CHARSET_SETUP_shift_jis;		  /*** Japanese (Shift_JIS) */
    UC_CHARSET_SETUP_euc_kr;		  /*** Korean		    */
    UC_CHARSET_SETUP_big5;		  /*** Taipei (Big5)	    */

    UC_CHARSET_SETUP_viscii;	/* Vietnamese (VISCII)  */
    UC_CHARSET_SETUP;		/* us-ascii */ /* 7 bit approximations */

    UC_CHARSET_SETUP_x_transparent;	  /*** Transparent	  */

    UC_CHARSET_SETUP_iso_8859_2;	/* ISO Latin 2          */
    UC_CHARSET_SETUP_cp852;	/* DosLatin2 (cp852)    */
    UC_CHARSET_SETUP_windows_1250;	/* WinLatin2 (cp1250)   */

    UC_CHARSET_SETUP_iso_8859_3;	/* ISO Latin 3          */
    UC_CHARSET_SETUP_iso_8859_4;	/* ISO Latin 4          */
    UC_CHARSET_SETUP_iso_8859_13;	/* ISO 8859-13 Baltic Rim */
    UC_CHARSET_SETUP_cp775;	/* DosBaltRim (cp775)   */
    UC_CHARSET_SETUP_windows_1257;	/* WinBaltRim (cp1257)  */
    UC_CHARSET_SETUP_iso_8859_5;	/* ISO 8859-5 Cyrillic  */
    UC_CHARSET_SETUP_cp866;	/* DosCyrillic (cp866)  */
    UC_CHARSET_SETUP_windows_1251;	/* WinCyrillic (cp1251) */
    UC_CHARSET_SETUP_koi8_r;	/* KOI8-R Cyrillic      */
    UC_CHARSET_SETUP_iso_8859_6;	/* ISO 8869-6 Arabic    */
    UC_CHARSET_SETUP_cp864;	/* DosArabic (cp864)    */
    UC_CHARSET_SETUP_windows_1256;	/* WinArabic (cp1256)   */
    UC_CHARSET_SETUP_iso_8859_14;	/* ISO 8859-14 Celtic   */
    UC_CHARSET_SETUP_iso_8859_7;	/* ISO 8859-7 Greek     */
    UC_CHARSET_SETUP_cp737;	/* DosGreek (cp737)     */
    UC_CHARSET_SETUP_cp869;	/* DosGreek2 (cp869)    */
    UC_CHARSET_SETUP_windows_1253;	/* WinGreek (cp1253)    */
    UC_CHARSET_SETUP_iso_8859_8;	/* ISO 8859-8 Hebrew    */
    UC_CHARSET_SETUP_cp862;	/* DosHebrew (cp862)    */
    UC_CHARSET_SETUP_windows_1255;	/* WinHebrew (cp1255)   */
    UC_CHARSET_SETUP_iso_8859_9;	/* ISO 8859-9 (Latin 5) */
    UC_CHARSET_SETUP_cp857;	/* DosTurkish (cp857) */
    UC_CHARSET_SETUP_iso_8859_10;	/* ISO 8859-10 North European */

    UC_CHARSET_SETUP_utf_8;		  /*** UNICODE UTF-8	  */
    UC_CHARSET_SETUP_mnemonic_ascii_0;	/* RFC 1345 w/o Intro   */
    UC_CHARSET_SETUP_mnemonic;	/* RFC 1345 Mnemonic    */
    UC_CHARSET_SETUP_cp866u;	/* Ukrainian Cyrillic (866) */
    UC_CHARSET_SETUP_koi8_u;	/* Ukrainian Cyrillic (koi8-u) */
    UC_CHARSET_SETUP_ptcp154;	/* Cyrillic-Asian (PT154) */

#ifdef EXP_CHARTRANS_AUTOSWITCH
#ifdef CAN_AUTODETECT_DISPLAY_CHARSET
#  ifdef __EMX__
    {
	unsigned UCode_t lst[3];
	unsigned UCode_t len, rc;

	rc = DosQueryCp(sizeof(lst), lst, &len);
	if (rc == 0) {
	    if (len >= 1)
		auto_display_charset = CpOrdinal(lst[0], 0);
#    ifdef CAN_SWITCH_DISPLAY_CHARSET
	    if (len >= 3) {
		codepages[0] = lst[0];
		codepages[1] = (lst[0] == lst[1] ? lst[2] : lst[1]);
		auto_other_display_charset = CpOrdinal(codepages[1], 1);
	    }
#    endif
	} else {
	    CTRACE((tfp, "DosQueryCp() returned %#lx=%lu.\n", rc, rc));
	}
    }
#  endif
#endif
#endif

/*
 * To add synonyms for any charset name check function UCGetLYhndl_byMIME in
 * this file.
 */

/* for coding/performance - easy to type: */
    LATIN1 = UCGetLYhndl_byMIME("iso-8859-1");
    US_ASCII = UCGetLYhndl_byMIME("us-ascii");
    UTF8_handle = UCGetLYhndl_byMIME("utf-8");
    TRANSPARENT = UCGetLYhndl_byMIME("x-transparent");
}

/*
 * Safe variant of UCGetLYhndl_byMIME, with blind recovery from typo in user
 * input:  lynx.cfg, userdefs.h, command line switches.
 */
int safeUCGetLYhndl_byMIME(const char *value)
{
    int i = UCGetLYhndl_byMIME(value);

    if (i == -1) {		/* was user's typo or not yet recognized value */
	i = LATIN1;		/* error recovery? */
	CTRACE((tfp, "safeUCGetLYhndl_byMIME: ISO-8859-1 assumed.\n"));
    }

    return (i);
}

#ifdef USE_LOCALE_CHARSET

#if defined(USE_LOCALE_CHARSET) && !defined(HAVE_LANGINFO_CODESET)
/*
 * This is a quick-and-dirty emulator of the nl_langinfo(CODESET)
 * function defined in the Single Unix Specification for those systems
 * (FreeBSD, etc.) that don't have one yet. It behaves as if it had
 * been called after setlocale(LC_CTYPE, ""), that is it looks at
 * the locale environment variables.
 *
 * http://www.opengroup.org/onlinepubs/7908799/xsh/langinfo.h.html
 *
 * Please extend it as needed and suggest improvements to the author.
 * This emulator will hopefully become redundant soon as
 * nl_langinfo(CODESET) becomes more widely implemented.
 *
 * Since the proposed Li18nux encoding name registry is still not mature,
 * the output follows the MIME registry where possible:
 *
 *   http://www.iana.org/assignments/character-sets
 *
 * A possible autoconf test for the availability of nl_langinfo(CODESET)
 * can be found in
 *
 *   http://www.cl.cam.ac.uk/~mgk25/unicode.html#activate
 *
 * Markus.Kuhn@cl.cam.ac.uk -- 2002-03-11
 * Permission to use, copy, modify, and distribute this software
 * for any purpose and without fee is hereby granted. The author
 * disclaims all warranties with regard to this software.
 *
 * Latest version:
 *
 *   http://www.cl.cam.ac.uk/~mgk25/ucs/langinfo.c
 */

/*
#include "langinfo.h"
*/
typedef int nl_item;

#define CODESET 1

#define C_CODESET "US-ASCII"	/* Return this as the encoding of the
				 * C/POSIX locale. Could as well one day
				 * become "UTF-8". */

#define digit(x) ((x) >= '0' && (x) <= '9')

static char buf[16];

static char *nl_langinfo(nl_item item)
{
    char *l, *p;

    if (item != CODESET)
	return NULL;

    if (((l = LYGetEnv("LC_ALL")) != 0) ||
	((l = LYGetEnv("LC_CTYPE")) != 0) ||
	((l = LYGetEnv("LANG")) != 0)) {
	/* check standardized locales */
	if (!strcmp(l, "C") || !strcmp(l, "POSIX"))
	    return C_CODESET;
	/* check for encoding name fragment */
	if (strstr(l, "UTF") || strstr(l, "utf"))
	    return "UTF-8";
	if ((p = strstr(l, "8859-"))) {
	    memcpy(buf, "ISO-8859-\0\0", 12);
	    p += 5;
	    if (digit(*p)) {
		buf[9] = *p++;
		if (digit(*p))
		    buf[10] = *p++;
		return buf;
	    }
	}
	if (strstr(l, "KOI8-R"))
	    return "KOI8-R";
	if (strstr(l, "KOI8-U"))
	    return "KOI8-U";
	if (strstr(l, "620"))
	    return "TIS-620";
	if (strstr(l, "2312"))
	    return "GB2312";
	if (strstr(l, "HKSCS"))
	    return "Big5HKSCS";	/* no MIME charset */
	if (strstr(l, "Big5") || strstr(l, "BIG5"))
	    return "Big5";
	if (strstr(l, "GBK"))
	    return "GBK";	/* no MIME charset */
	if (strstr(l, "18030"))
	    return "GB18030";	/* no MIME charset */
	if (strstr(l, "Shift_JIS") || strstr(l, "SJIS"))
	    return "Shift_JIS";
	/* check for conclusive modifier */
	if (strstr(l, "euro"))
	    return "ISO-8859-15";
	/* check for language (and perhaps country) codes */
	if (strstr(l, "zh_TW"))
	    return "Big5";
	if (strstr(l, "zh_HK"))
	    return "Big5HKSCS";	/* no MIME charset */
	if (strstr(l, "zh"))
	    return "GB2312";
	if (strstr(l, "ja"))
	    return "EUC-JP";
	if (strstr(l, "ko"))
	    return "EUC-KR";
	if (strstr(l, "ru"))
	    return "KOI8-R";
	if (strstr(l, "uk"))
	    return "KOI8-U";
	if (strstr(l, "pl") || strstr(l, "hr") ||
	    strstr(l, "hu") || strstr(l, "cs") ||
	    strstr(l, "sk") || strstr(l, "sl"))
	    return "ISO-8859-2";
	if (strstr(l, "eo") || strstr(l, "mt"))
	    return "ISO-8859-3";
	if (strstr(l, "el"))
	    return "ISO-8859-7";
	if (strstr(l, "he"))
	    return "ISO-8859-8";
	if (strstr(l, "tr"))
	    return "ISO-8859-9";
	if (strstr(l, "th"))
	    return "TIS-620";	/* or ISO-8859-11 */
	if (strstr(l, "lt"))
	    return "ISO-8859-13";
	if (strstr(l, "cy"))
	    return "ISO-8859-14";
	if (strstr(l, "ro"))
	    return "ISO-8859-2";	/* or ISO-8859-16 */
	if (strstr(l, "am") || strstr(l, "vi"))
	    return "UTF-8";
	/* Send me further rules if you like, but don't forget that we are
	 * *only* interested in locale naming conventions on platforms
	 * that do not already provide an nl_langinfo(CODESET) implementation. */
	return "ISO-8859-1";	/* should perhaps be "UTF-8" instead */
    }
    return C_CODESET;
}
#endif /* defined(USE_LOCALE_CHARSET) && !defined(HAVE_LANGINFO_CODESET) */

/*
 * If LYLocaleCharset is true, use the current locale to lookup a MIME name
 * that corresponds, and use that as the display charset.  This feature is
 * experimental because while nl_langinfo(CODESET) itself is standardized,
 * the return values and their relationship to the locale value is not.
 * GNU libiconv happens to give useful values, but other implementations are
 * not guaranteed to do this.
 *
 * Not all Linux versions provide useful information.  GNU libc 2.2 returns
 *	"ANSI_X3.4-1968"
 * whether locale is POSIX or en_US.UTF-8.
 *
 * Another possible thing to investigate is the locale_charset() function
 * provided in libiconv 1.5.1.
 */
void LYFindLocaleCharset(void)
{
    char *name;

    CTRACE((tfp, "LYFindLocaleCharset(%d)\n", LYLocaleCharset));
    name = nl_langinfo(CODESET);

    if (name != 0) {
	int value = UCGetLYhndl_byMIME(name);

	if (value >= 0) {
	    linedrawing_char_set = value;
	    CTRACE((tfp, "Found name \"%s\" -> %d\n", name, value));
	    /*
	     * If no locale was set, we will get the POSIX character set, which
	     * in Lynx is treated as US-ASCII.  However, Lynx's longstanding
	     * behavior has been to default to ISO-8859-1.  So we treat that
	     * encoding specially.  Otherwise, if LOCALE_CHARSET is set, then
	     * we will use the locale encoding -- unless overridden by the
	     * ASSUME_CHARSET value and/or command-line option.
	     */
	    if (LYLocaleCharset) {
		CTRACE((tfp, "...prior LocaleCharset '%s'\n", NonNull(UCAssume_MIMEcharset)));
		if (value == US_ASCII) {
		    CTRACE((tfp, "...prefer existing charset to ASCII\n"));
		} else if (assumed_charset) {
		    CTRACE((tfp, "...already assumed-charset\n"));
		} else {
		    current_char_set = linedrawing_char_set;
		    UCLYhndl_for_unspec = current_char_set;
		    StrAllocCopy(UCAssume_MIMEcharset, name);
		    CTRACE((tfp, "...using LocaleCharset '%s'\n", NonNull(UCAssume_MIMEcharset)));
		}
	    }
	} else {
	    CTRACE((tfp, "Cannot find a handle for MIME name \"%s\"\n", name));
	}
    } else {
	CTRACE((tfp, "Cannot find a MIME name for locale\n"));
    }
}
#endif /* USE_LOCALE_CHARSET */

BOOL UCScanCode(UCode_t *target, const char *source, BOOL isHex)
{
    BOOL status = FALSE;
    long lcode;
    char *endptr;

    errno = 0;
    *target = 0;
    lcode = strtol(source, &endptr, isHex ? 16 : 10);
    if (lcode >= 0
	&& (endptr > source)
#if defined(ERANGE) && defined(LONG_MAX) && defined(LONG_MIN)
	&& (errno != ERANGE || (lcode != LONG_MAX && lcode != LONG_MIN))
#else
	&& (endptr - source) < (isHex ? 8 : 10)
#endif
	&& (endptr != 0)
	&& (*endptr == '\0')) {
	*target = (UCode_t) lcode;
	status = TRUE;
    }
    return status;
}
