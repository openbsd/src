/*	$OpenBSD: arcbios.h,v 1.1 2004/08/06 21:12:18 pefo Exp $ */
/*	$NetBSD: arcbios.h,v 1.5 2004/04/10 19:32:53 pooka Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * The ARC BIOS (which is similar, but not 100% compatible with SGI ARCS)
 * specification can be found at:
 *
 *	http://www.microsoft.com/hwdev/download/respec/riscspec.zip
 */

#ifndef _ARCBIOS_H_
#define _ARCBIOS_H_

#define	ARCBIOS_STDIN		0
#define	ARCBIOS_STDOUT		1

#define	ARCBIOS_PAGESIZE	4096

/* ARC BIOS status codes. */
#define	ARCBIOS_ESUCCESS	0	/* Success */
#define	ARCBIOS_E2BIG		1	/* argument list too long */
#define	ARCBIOS_EACCES		2	/* permission denied */
#define	ARCBIOS_EAGAIN		3	/* resource temporarily unavailable */
#define	ARCBIOS_EBADF		4	/* bad file number */
#define	ARCBIOS_EBUSY		5	/* device or resource busy */
#define	ARCBIOS_EFAULT		6	/* bad address */
#define	ARCBIOS_EINVAL		7	/* invalid argument */
#define	ARCBIOS_EIO		8	/* I/O error */
#define	ARCBIOS_EISDIR		9	/* is a directory */
#define	ARCBIOS_EMFILE		10	/* too many open files */
#define	ARCBIOS_EMLINK		11	/* too many links */
#define	ARCBIOS_ENAMETOOLONG	12	/* file name too long */
#define	ARCBIOS_ENODEV		13	/* no such device */
#define	ARCBIOS_ENOENT		14	/* no such file or directory */
#define	ARCBIOS_ENOEXEC		15	/* exec format error */
#define	ARCBIOS_ENOMEM		16	/* out of memory */
#define	ARCBIOS_ENOSPC		17	/* no space left on device */
#define	ARCBIOS_ENOTDIR		18	/* not a directory */
#define	ARCBIOS_ENOTTY		19	/* not a typewriter */
#define	ARCBIOS_ENXIO		20	/* media not loaded */
#define	ARCBIOS_EROFS		21	/* read-only file system */
#if defined(__sgi__)
#define	ARCBIOS_EADDRNOTAVAIL	31	/* address not available */
#define	ARCBIOS_ETIMEDOUT	32	/* operation timed out */
#define	ARCBIOS_ECONNABORTED	33	/* connection aborted */
#define	ARCBIOS_ENOCONNECT	34	/* not connected */
#endif /* __sgi__ */

/*
 * 4.2.2: System Parameter Block
 */
struct arcbios_spb {
	uint32_t	SPBSignature;
	uint32_t	SPBLength;
	uint16_t	Version;
	uint16_t	Revision;
	void		*RestartBlock;
	void		*DebugBlock;
	void		*GEVector;
	void		*UTLBMissVector;
	uint32_t	FirmwareVectorLength;
	void		*FirmwareVector;
	uint32_t	PrivateVectorLength;
	void		*PrivateVector;
	uint32_t	AdapterCount;
	uint32_t	AdapterType;
	uint32_t	AdapterVectorLength;
	void		*AdapterVector;
};

#define	ARCBIOS_SPB_SIGNATURE	0x53435241	/* A R C S */
#define	ARCBIOS_SPB_SIGNATURE_1	0x41524353	/* S C R A */

/*
 * 4.2.5: System Configuration Data
 */
struct arcbios_component {
	uint32_t	Class;
	uint32_t	Type;
	uint32_t	Flags;
	uint16_t	Version;
	uint16_t	Revision;
	uint32_t	Key;
	uint32_t	AffinityMask;
	uint32_t	ConfigurationDataSize;
	uint32_t	IdentifierLength;
	char		*Identifier;
};

/* 
 * SGI ARCS likes to be `special', so it moved some of the class/type 
 * numbers around from the ARC standard definitions.
 */
#if defined(__sgi__)
/* Component Class */
#define	COMPONENT_CLASS_SystemClass		0
#define	COMPONENT_CLASS_ProcessorClass		1
#define	COMPONENT_CLASS_CacheClass		2
#define	COMPONENT_CLASS_MemoryClass		3
#define	COMPONENT_CLASS_AdapterClass		4
#define	COMPONENT_CLASS_ControllerClass		5
#define	COMPONENT_CLASS_PeripheralClass		6
#else
/* Component Class */
#define	COMPONENT_CLASS_SystemClass		0
#define	COMPONENT_CLASS_ProcessorClass		1
#define	COMPONENT_CLASS_CacheClass		2
#define	COMPONENT_CLASS_AdapterClass		3
#define	COMPONENT_CLASS_ControllerClass		4
#define	COMPONENT_CLASS_PeripheralClass		5
#define	COMPONENT_CLASS_MemoryClass		6
#endif

/* Component Types */
#if defined(__sgi__)
/* System Class */
#define	COMPONENT_TYPE_ARC			0

/* Processor Class */
#define	COMPONENT_TYPE_CPU			1
#define	COMPONENT_TYPE_FPU			2

/* Cache Class */
#define	COMPONENT_TYPE_PrimaryICache		3
#define	COMPONENT_TYPE_PrimaryDCache		4
#define	COMPONENT_TYPE_SecondaryICache		5
#define	COMPONENT_TYPE_SecondaryDCache		6
#define	COMPONENT_TYPE_SecondaryCache		7

/* Memory Class */
#define	COMPONENT_TYPE_MemoryUnit		8

/* Adapter Class */
#define	COMPONENT_TYPE_EISAAdapter		9
#define	COMPONENT_TYPE_TCAdapter		10
#define	COMPONENT_TYPE_SCSIAdapter		11
#define	COMPONENT_TYPE_DTIAdapter		12
#define	COMPONENT_TYPE_MultiFunctionAdapter	13

/* Controller Class */
#define	COMPONENT_TYPE_DiskController		14
#define	COMPONENT_TYPE_TapeController		15
#define	COMPONENT_TYPE_CDROMController		16
#define	COMPONENT_TYPE_WORMController		17
#define	COMPONENT_TYPE_SerialController		18
#define	COMPONENT_TYPE_NetworkController	19
#define	COMPONENT_TYPE_DisplayController	20
#define	COMPONENT_TYPE_ParallelController	21
#define	COMPONENT_TYPE_PointerController	22
#define	COMPONENT_TYPE_KeyboardController	23
#define	COMPONENT_TYPE_AudioController		24
#define	COMPONENT_TYPE_OtherController		25

/* Peripheral Class */
#define	COMPONENT_TYPE_DiskPeripheral		26
#define	COMPONENT_TYPE_FloppyDiskPeripheral	27
#define	COMPONENT_TYPE_TapePeripheral		28
#define	COMPONENT_TYPE_ModemPeripheral		29
#define	COMPONENT_TYPE_MonitorPeripheral	30
#define	COMPONENT_TYPE_PrinterPeripheral	31
#define	COMPONENT_TYPE_PointerPeripheral	32
#define	COMPONENT_TYPE_KeyboardPeripheral	33
#define	COMPONENT_TYPE_TerminalPeripheral	34
#define	COMPONENT_TYPE_LinePeripheral		35
#define	COMPONENT_TYPE_NetworkPeripheral	36
#define	COMPONENT_TYPE_OtherPeripheral		37
#else /* not __sgi__ */
/* System Class */
#define	COMPONENT_TYPE_ARC			0

/* Processor Class */
#define	COMPONENT_TYPE_CPU			1
#define	COMPONENT_TYPE_FPU			2

/* Cache Class */
#define	COMPONENT_TYPE_PrimaryICache		3
#define	COMPONENT_TYPE_PrimaryDCache		4
#define	COMPONENT_TYPE_SecondaryICache		5
#define	COMPONENT_TYPE_SecondaryDCache		6
#define	COMPONENT_TYPE_SecondaryCache		7

/* Adapter Class */
#define	COMPONENT_TYPE_EISAAdapter		8
#define	COMPONENT_TYPE_TCAdapter		9
#define	COMPONENT_TYPE_SCSIAdapter		10
#define	COMPONENT_TYPE_DTIAdapter		11
#define	COMPONENT_TYPE_MultiFunctionAdapter	12

/* Controller Class */
#define	COMPONENT_TYPE_DiskController		13
#define	COMPONENT_TYPE_TapeController		14
#define	COMPONENT_TYPE_CDROMController		15
#define	COMPONENT_TYPE_WORMController		16
#define	COMPONENT_TYPE_SerialController		17
#define	COMPONENT_TYPE_NetworkController	18
#define	COMPONENT_TYPE_DisplayController	19
#define	COMPONENT_TYPE_ParallelController	20
#define	COMPONENT_TYPE_PointerController	21
#define	COMPONENT_TYPE_KeyboardController	22
#define	COMPONENT_TYPE_AudioController		23
#define	COMPONENT_TYPE_OtherController		24

/* Peripheral Class */
#define	COMPONENT_TYPE_DiskPeripheral		25
#define	COMPONENT_TYPE_FloppyDiskPeripheral	26
#define	COMPONENT_TYPE_TapePeripheral		27
#define	COMPONENT_TYPE_ModemPeripheral		28
#define	COMPONENT_TYPE_MonitorPeripheral	29
#define	COMPONENT_TYPE_PrinterPeripheral	30
#define	COMPONENT_TYPE_PointerPeripheral	31
#define	COMPONENT_TYPE_KeyboardPeripheral	32
#define	COMPONENT_TYPE_TerminalPeripheral	33
#define	COMPONENT_TYPE_OtherPeripheral		34
#define	COMPONENT_TYPE_LinePeripheral		35
#define	COMPONENT_TYPE_NetworkPeripheral	36

/* Memory Class */
#define	COMPONENT_TYPE_MemoryUnit		37
#endif

/* Component flags */
#define	COMPONENT_FLAG_Failed			1
#define	COMPONENT_FLAG_ReadOnly			2
#define	COMPONENT_FLAG_Removable		4
#define	COMPONENT_FLAG_ConsoleIn		8
#define	COMPONENT_FLAG_ConsoleOut		16
#define	COMPONENT_FLAG_Input			32
#define	COMPONENT_FLAG_Output			64

/* Key for Cache: */
#define	COMPONENT_KEY_Cache_CacheSize(x)				\
	(ARCBIOS_PAGESIZE << ((x) & 0xffff))
#define	COMPONENT_KEY_Cache_LineSize(x)					\
	(1U << (((x) >> 16) & 0xff))
#define	COMPONENT_KEY_Cache_RefillSize(x)				\
	(((x) >> 24) & 0xff)

/*
 * ARC system ID
 */
#define	ARCBIOS_SYSID_FIELDLEN		8
struct arcbios_sysid {
	char		VendorId[ARCBIOS_SYSID_FIELDLEN];
	char		ProductId[ARCBIOS_SYSID_FIELDLEN];
};

/*
 * ARC memory descriptor
 */
struct arcbios_mem {
	uint32_t	Type;
	uint32_t	BasePage;
	uint32_t	PageCount;
};

#if defined(__sgi__)
#define	ARCBIOS_MEM_ExceptionBlock		0
#define	ARCBIOS_MEM_SystemParameterBlock	1
#define	ARCBIOS_MEM_FreeContiguous		2
#define	ARCBIOS_MEM_FreeMemory			3
#define	ARCBIOS_MEM_BadMemory			4
#define	ARCBIOS_MEM_LoadedProgram		5
#define	ARCBIOS_MEM_FirmwareTemporary		6
#define	ARCBIOS_MEM_FirmwarePermanent		7
#elif defined(arc)
#define	ARCBIOS_MEM_ExceptionBlock		0
#define	ARCBIOS_MEM_SystemParameterBlock	1
#define	ARCBIOS_MEM_FreeMemory			2
#define	ARCBIOS_MEM_BadMemory			3
#define	ARCBIOS_MEM_LoadedProgram		4
#define	ARCBIOS_MEM_FirmwareTemporary		5
#define	ARCBIOS_MEM_FirmwarePermanent		6
#define	ARCBIOS_MEM_FreeContiguous		7
#endif

/*
 * ARC display status
 */
struct arcbios_dsp_stat {
	uint16_t	CursorXPosition;
	uint16_t	CursorYPosition;
	uint16_t	CursorMaxXPosition;
	uint16_t	CursorMaxYPosition;
	uint8_t		ForegroundColor;
	uint8_t		BackgroundColor;
	uint8_t		HighIntensity;
	uint8_t		Underscored;
	uint8_t		ReverseVideo;
};

/*
 * ARC firmware vector
 */
struct arcbios_fv {
	uint32_t	(*Load)(
			    char *,		/* image to load */
			    uint32_t,		/* top address */
			    uint32_t,		/* entry address */
			    uint32_t *);	/* low address */

	uint32_t	(*Invoke)(
			    uint32_t,		/* entry address */
			    uint32_t,		/* stack address */
			    uint32_t,		/* argc */
			    char **,		/* argv */
			    char **);		/* envp */

	uint32_t	(*Execute)(
			    char *,		/* image path */
			    uint32_t,		/* argc */
			    char **,		/* argv */
			    char **);		/* envp */

	void		(*Halt)(void)
			    __attribute__((__noreturn__));

	void		(*PowerDown)(void)
			    __attribute__((__noreturn__));

	void		(*Restart)(void)
			    __attribute__((__noreturn__));

	void		(*Reboot)(void)
			    __attribute__((__noreturn__));

	void		(*EnterInteractiveMode)(void)
			    __attribute__((__noreturn__));
#if defined(__sgi__)
	void		*reserved0;
#else
	void		(*ReturnFromMain)(void)
			    __attribute__((__noreturn__));
#endif
	void		*(*GetPeer)(
			    void *);		/* component */

	void		*(*GetChild)(
			    void *);		/* component */

	void		*(*GetParent)(
			    void *);		/* component */

	uint32_t	(*GetConfigurationData)(
			    void *,		/* configuration data */
			    void *);		/* component */

	void		*(*AddChild)(
			    void *,		/* component */
			    void *);		/* new component */

	uint32_t	(*DeleteComponent)(
			    void *);		/* component */

	uint32_t	(*GetComponent)(
			    char *);		/* path */

	uint32_t	(*SaveConfiguration)(void);

	void		*(*GetSystemId)(void);

	void		*(*GetMemoryDescriptor)(
			    void *);		/* memory descriptor */
#if defined(__sgi__)
	void		*reserved1;
#else
	void		(*Signal)(
			    uint32_t,		/* signal number */
			    void *);		/* handler */
#endif
	void		*(*GetTime)(void);

	uint32_t	(*GetRelativeTime)(void);

	uint32_t	(*GetDirectoryEntry)(
			    uint32_t,		/* file ID */
			    void *,		/* directory entry */
			    uint32_t,		/* length */
			    uint32_t *);	/* count */

	uint32_t	(*Open)(
			    char *,		/* path */
			    uint32_t,		/* open mode */
			    uint32_t *);	/* file ID */

	uint32_t	(*Close)(
			    uint32_t);		/* file ID */

	uint32_t	(*Read)(
			    uint32_t,		/* file ID */
			    void *,		/* buffer */
			    uint32_t,		/* length */
			    uint32_t *);	/* count */

	uint32_t	(*GetReadStatus)(
			    uint32_t);		/* file ID */

	uint32_t	(*Write)(
			    uint32_t,		/* file ID */
			    void *,		/* buffer */
			    uint32_t,		/* length */
			    uint32_t *);	/* count */

	uint32_t	(*Seek)(
			    uint32_t,		/* file ID */
			    int64_t *,		/* offset */
			    uint32_t);		/* whence */

	uint32_t	(*Mount)(
			    char *,		/* path */
			    uint32_t);		/* operation */

	char		*(*GetEnvironmentVariable)(
			    char *);		/* variable */

	uint32_t	(*SetEnvironmentVariable)(
			    char *,		/* variable */
			    char *);		/* contents */

	uint32_t	(*GetFileInformation)(
			    uint32_t,		/* file ID */
			    void *);		/* XXX */

	uint32_t	(*SetFileInformation)(
			    uint32_t,		/* file ID */
			    uint32_t,		/* XXX */
			    uint32_t);		/* XXX */

	void		(*FlushAllCaches)(void);
#if !defined(__sgi__)
	uint32_t	(*TestUnicode)(
			    uint32_t,		/* file ID */
			    uint16_t);		/* unicode character */

	void		*(*GetDisplayStatus)(
			    uint32_t);		/* file ID */
#endif
};

#endif /* _ARCBIOS_H_ */
