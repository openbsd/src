/*	$NetBSD: nubus.h,v 1.12 1996/01/12 04:52:58 briggs Exp $	*/

/*
 * Copyright (c) 1995 Allen Briggs.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Allen Briggs.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Nubus cards in Macintoshes are identified by 4 16-bit numbers:
 * Category:	What is the main purpose of this card?
 * Type:	Within this overall category, what kind of card?
 * DrSW:	What software interface does it use?
 * DrHW:	What specific hardware is it?
 *
 * For example, the "Toby Frame Buffer" display card is
 *     Category 3 (display)
 *     Type     1 (video)
 *     DrSW     1 (Apple)
 * and DrHW     1 (TFB).
 */

#include <machine/cpu.h>

#define NUBUS_CATEGORY_BOARD	0x0001

#define NUBUS_CATEGORY_DISPLAY	0x0003
#define  NUBUS_TYPE_VIDEO	0x0001
#define  NUBUS_TYPE_LCD		0x0002
#define   NUBUS_DRSW_APPLE	0x0001
#define    NUBUS_DRHW_TFB	0x0001
#define    NUBUS_DRHW_M2HRVC	0x0013
#define    NUBUS_DRHW_MICRON	0x0146

#define NUBUS_CATEGORY_NETWORK	0x0004
#define  NUBUS_TYPE_ETHERNET	0x0001
#define   NUBUS_DRSW_3COM	0x0000
#define   NUBUS_DRSW_GATOR	0x0103
#define    NUBUS_DRHW_INTERLAN	0x0100
#define    NUBUS_DRHW_KINETICS	0x0106
#define   NUBUS_DRSW_ASANTE	0x0104
#define   NUBUS_DRSW_TECHWORKS	0x0109
#define    NUBUS_DRHW_SONIC	0x0110
#define   NUBUS_DRSW_FARALLON	0x010C
#define   NUBUS_DRSW_FOCUS	0x011A

#define NUBUS_CATEGORY_COMMUNICATIONS	0x0006
#define  NUBUS_TYPE_RS232	0x0002
#define   NUBUS_DRSW_HDS	0x0102
#define    NUBUS_DRHW_HDS	0x0102

#define NUBUS_CATEGORY_FONT	0x0009	/* KanjiTalk Font Card? */

#define NUBUS_CATEGORY_CPU	0x000A
#define  NUBUS_TYPE_68000	0x0002
#define  NUBUS_TYPE_68020	0x0003
#define  NUBUS_TYPE_68030	0x0004
#define  NUBUS_TYPE_68040	0x0005

/*
 * This is the same as Apple's Format Block for a card, with the
 * addition of a pointer to the base of the NuBUS slot.
 *
 * This basically describes a nubus card--this structure is held in the last
 * N bytes of each valid card's declaration ROM.
 */
typedef struct _nubus_slot {
	u_long		top;
	u_int8_t	slot;
	u_int8_t	bytelanes;
	u_int8_t	step;
	u_int32_t	test_pattern;
	u_int8_t	format;
	u_int8_t	revision_level;
	u_int32_t	crc;
	u_int32_t	length;
	u_int32_t	directory_offset;
} nubus_slot;

/*
 * Just a structure to ease comparison of type for drivers, etc.
 */
typedef struct _nubus_type {
	u_int16_t	category;
	u_int16_t	type;
	u_int16_t	drsw;
	u_int16_t	drhw;
} nubus_type;

/*
 * nubus_dir is a structure that describes a nubus directory.
 * The nubus*dir() functions should be used to traverse this.
 */
typedef struct _nubus_dir {
	u_int32_t	dirbase;
	u_int32_t	curr_ent;
} nubus_dir;

/*
 * This is the equivalent of an Apple sResource directory entry
 * with the addition of a pointer to itself (essentially) for easy
 * calculation of jump to indirect data.
 */
typedef struct _nubus_dirent {
	u_int32_t	myloc;
	u_int8_t	rsrc_id;
	u_int32_t	offset;
} nubus_dirent;

/*
 * This is the equivalent of an Apple sResource with the addition of
 * a pointer back to the sResource directory from whence we came.
 *
 * According to the Apple documentation, each sResource is of one of the
 * following forms:
 *	all:	bits 31-24	Identification number
 *
 *	offset:	bits 23-0	Offset to long data, cString, sBlock, etc.
 *	word:	bits 23-16	0x00
 *		bits 15-0	word data
 *	byte:	bits 23-8	0x0000
 *		bits 7-0	byte data
 *
 * The last resource has id = 0xff and data = 0x000000.
 */
typedef struct _nubus_rsrc {
	u_int32_t	myloc;
	u_int8_t	id;
	u_int32_t	data;
} nubus_rsrc;

/* Resource IDs for NUBUS_CATEGORY_* (All) */
#define NUBUS_RSRC_TYPE		0x01	/* Type (required)		*/
#define NUBUS_RSRC_NAME		0x02	/* Name (required)		*/
#define NUBUS_RSRC_ICON		0x03	/* Icon				*/
#define NUBUS_RSRC_DRVRDIR	0x04	/* Driver directory		*/
#define NUBUS_RSRC_LOADREC	0x05	/* Load record for resource	*/
#define NUBUS_RSRC_BOOTREC	0x06	/* Boot record			*/
#define NUBUS_RSRC_FLAGS	0x07	/* sResource Flags		*/
#define NUBUS_RSRC_HWDEVID	0x08	/* Hardware device ID		*/
#define NUBUS_RSRC_MINOR_BASEOS	0x0A	/* Offset to hw in std space	*/
#define NUBUS_RSRC_MINOR_LENGTH	0x0B	/* Length of std space		*/
#define NUBUS_RSRC_MAJOR_BASEOS	0x0C	/* Offset to hw in super space	*/
#define NUBUS_RSRC_MAJOR_LENGTH	0x0D	/* Length of super space	*/
#define NUBUS_RSRC_CICN		0x0F	/* Color icon			*/
#define NUBUS_RSRC_ICL8		0x10	/* 8-bit icon data		*/
#define NUBUS_RSRC_ICL4		0x11	/* 4-bit icon data		*/

/* Resource IDs for NUBUS_CATEGORY_DISPLAY */
#define NUBUS_RSRC_GAMMADIR	0x40	/* ID for gamma directory	*/
#define NUBUS_RSRC_VIDNAMES	0x41	/* ID for video name directory	*/
#define NUBUS_RSRC_FIRSTMODE	0x80	/* ID for first mode (1-bit)	*/
#define NUBUS_RSRC_SECONDMODE	0x81	/* ID for 2nd mode (2-bit)	*/
#define NUBUS_RSRC_THIRDMODE	0x82	/* ID for 3rd mode (4-bit)	*/
#define NUBUS_RSRC_FOURTHMODE	0x83	/* ID for 4th mode (8-bit)	*/
#define NUBUS_RSRC_FIFTHMODE	0x84	/* ID for 5th mode (16-bit)	*/
#define NUBUS_RSRC_SIXTHMODE	0x85	/* ID for 6th mode (32-bit)	*/

/* Resource IDs for NUBUS_CATEGORY_BOARD */
#define NUBUS_RSRC_BOARDID	0x20	/* Board ID			*/
#define NUBUS_RSRC_PRAMINITDATA	0x21	/* Private board data for PRAM	*/
#define NUBUS_RSRC_PRIMARYINIT	0x22	/* Primary init record		*/
#define NUBUS_RSRC_TIMEOUTCONST	0x23	/* Timeout constant		*/
#define NUBUS_RSRC_VENDORINFO	0x24	/* Vendor info list		*/
#define NUBUS_RSRC_BOARDFLAGS	0x25	/* Board flags			*/
#define NUBUS_RSRC_SECONDINIT	0x26	/* Secondary init record	*/

#define NUBUS_RSRC_VEND_ID	0x01	/* Card vendor's design ID	*/
#define NUBUS_RSRC_VEND_SERIAL	0x02	/* Card's serial number		*/
#define NUBUS_RSRC_VEND_REV	0x03	/* Card design's revision level	*/
#define NUBUS_RSRC_VEND_PART	0x04	/* Card part number		*/
#define NUBUS_RSRC_VEND_DATE	0x05	/* Card revision date		*/

typedef struct _NUBUS_DRIVER {
	u_int8_t	drvr_id;
	u_int32_t	offset;
} NUBUS_DRIVER;

typedef struct _NUBUS_BLOCK {
	u_int32_t	size;		/* Size of block of data	*/
	caddr_t		data;		/* Pointer to data		*/
} NUBUS_BLOCK;

typedef struct _NUBUS_EXEC_BLOCK {
	u_int32_t	size;		/* Size of total block - 4 	*/
	u_int8_t	revision;	/* Always 0x2			*/
	u_int8_t	cpu;		/* Which processor?		*/
	u_int32_t	code_offset;	/* Offset base to start of code	*/
	caddr_t		code;		/* pointer to base of code.	*/
} NUBUS_EXEC_BLOCK;

#define NUBUS_EXEC_CPU_68000	1
#define NUBUS_EXEC_CPU_68020	2
#define NUBUS_EXEC_CPU_68030	3
#define NUBUS_EXEC_CPU_68040	4

#define NUBUS_MIN_SLOT		0x9
#define NUBUS_MAX_SLOT		0xE
#define	NUBUS_INT_VIDEO_PSUEDO_SLOT 0xF
#define NUBUS_ROM_TEST_PATTERN	0x5A932BC7

#define NUBUS_BASE_TO_SLOT(x)	(((((x)-NuBusBase) >> 24) & 0x0F) + \
				 NUBUS_MIN_SLOT)
#define NUBUS_SLOT_TO_BASE(x)	(NuBusBase + \
				 ((((x)-NUBUS_MIN_SLOT) & 0xF) << 24))
#define NUBUS_VIRT_TO_PHYS(x)	((x - NuBusBase) + \
				 ((0xF0 | NUBUS_MIN_SLOT) << 24))

struct nubus_softc {
	struct	device	sc_dev;
};

void	nubus_get_main_dir __P((nubus_slot *slot, nubus_dir *dir_return));
int	nubus_find_rsrc __P((nubus_slot *slot, nubus_dir *dir, u_int8_t rsrcid,
			     nubus_dirent *dirent_return));
void	nubus_get_dir_from_rsrc __P((nubus_slot *slot, nubus_dirent *dirent,
				     nubus_dir *dir_return));
int	nubus_get_ind_data __P((nubus_slot *slot, nubus_dirent *dirent,
				caddr_t data_return, int nbytes));
int	nubus_get_c_string __P((nubus_slot *slot, nubus_dirent *dirent,
				caddr_t data_return, int max_bytes));
char	*nubus_get_vendor __P((nubus_slot *slot, int rsrc));
char	*nubus_get_card_name __P((nubus_slot *slot));

