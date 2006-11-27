/* $OpenBSD: vesabiosreg.h,v 1.1 2006/11/27 18:04:28 gwk Exp $ */

/*
 * Written by M. Drochner
 * Public domain.
 */


#ifndef _VESABIOSREG_H
#define _VESABIOSREG_H

struct modeinfoblock {
	/* Mandatory information for all VBE revisions */
	uint16_t ModeAttributes;
	uint8_t WinAAttributes, WinBAttributes;
	uint16_t WinGranularity, WinSize, WinASegment, WinBSegment;
	uint32_t WinFuncPtr;
	uint16_t BytesPerScanLine;
	/* Mandatory information for VBE 1.2 and above */
	uint16_t XResolution, YResolution;
	uint8_t XCharSize, YCharSize, NumberOfPlanes, BitsPerPixel;
	uint8_t NumberOfBanks, MemoryModel, BankSize, NumberOfImagePages;
	uint8_t Reserved1;
	/* Direct Color fields
	   (required for direct/6 and YUV/7 memory models) */
	uint8_t RedMaskSize, RedFieldPosition;
	uint8_t GreenMaskSize, GreenFieldPosition;
	uint8_t BlueMaskSize, BlueFieldPosition;
	uint8_t RsvdMaskSize, RsvdFieldPosition;
	uint8_t DirectColorModeInfo;
	/* Mandatory information for VBE 2.0 and above */
	uint32_t PhysBasePtr;
#ifdef VBE_2_0
	uint32_t OffScreenMemOffset;
	uint16_t OffScreenMemSize;
	uint8_t Reserved2[206];
#else
	uint32_t Reserved2;
	uint16_t Reserved3;

	/* Mandatory information for VBE 3.0 and above */
	uint16_t LinBytesPerScanLine;
	uint8_t BnkNumberOfImagePages;
	uint8_t LinNumberOfImagePages;
	uint8_t LinRedMaskSize, LinRedFieldPosition;
	uint8_t LinGreenMaskSize, LinGreenFieldPosition;
	uint8_t LinBlueMaskSize, LinBlueFieldPosition;
	uint8_t LinRsvdMaskSize, LinRsvdFieldPosition;
	uint32_t MaxPixelClock;
	uint8_t Reserved4[189];
#endif
} __attribute__ ((packed));

struct paletteentry {
	uint8_t Blue;
	uint8_t Green;
	uint8_t Red;
	uint8_t Alignment;
} __attribute__ ((packed));

#endif /* !_VESABIOSREG_H */
