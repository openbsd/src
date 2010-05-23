/*	$OpenBSD: arcbios.h,v 1.16 2010/05/23 21:29:05 deraadt Exp $	*/
/*-
 * Copyright (c) 1996 M. Warner Losh.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>

typedef struct arc_sid
{
	char vendor[8];
	char prodid[8];
} arc_sid_t;

typedef enum arc_config_class
{
	arc_SystemClass,
	arc_ProcessorClass,
	arc_CacheClass,
	arc_AdapterClass,
	arc_ControllerClass,
	arc_PeripheralClass,
	arc_MemoryClass
} arc_config_class_t;

typedef enum arc_config_type
{
	arc_System,

	arc_CentralProcessor,
	arc_FloatingPointProcessor,

	arc_PrimaryIcache,
	arc_PrimaryDcache,
	arc_SecondaryIcache,
	arc_SecondaryDcache,
	arc_SecondaryCache,

#ifdef __sgi__
	arc_SystemMemory,
#endif

	arc_EisaAdapter,		/* Eisa adapter         */
	arc_TcAdapter,			/* Turbochannel adapter */
	arc_ScsiAdapter,		/* SCSI adapter         */
	arc_DtiAdapter,			/* AccessBus adapter    */
	arc_MultiFunctionAdapter,

	arc_DiskController,
	arc_TapeController,
	arc_CdromController,
	arc_WormController,
	arc_SerialController,
	arc_NetworkController,
	arc_DisplayController,
	arc_ParallelController,
	arc_PointerController,
	arc_KeyboardController,
	arc_AudioController,
	arc_OtherController,		/* denotes a controller not otherwise defined */

	arc_DiskPeripheral,
	arc_FloppyDiskPeripheral,
	arc_TapePeripheral,
	arc_ModemPeripheral,
	arc_MonitorPeripheral,
	arc_PrinterPeripheral,
	arc_PointerPeripheral,
	arc_KeyboardPeripheral,
	arc_TerminalPeripheral,
#ifdef __arc__
	arc_OtherPeripheral,		/* denotes a peripheral not otherwise defined   */
#endif
	arc_LinePeripheral,
	arc_NetworkPeripheral,
#ifdef __arc__
	arc_SystemMemory,
#endif
#ifdef __sgi__
	arc_OtherPeripheral,		/* denotes a peripheral not otherwise defined   */
#endif
} arc_config_type_t;

typedef u_int32_t arc_dev_flags_t;

#define	ARCBIOS_DEVFLAGS_FAILED		0x01
#define	ARCBIOS_DEVFLAGS_READONLY	0x02
#define	ARCBIOS_DEVFLAGS_REMOVABLE	0x04
#define	ARCBIOS_DEVFLAGS_CONSOLE_INPUT	0x08
#define	ARCBIOS_DEVFLAGS_CONSOLE_OUTPUT	0x10
#define	ARCBIOS_DEVFLAGS_INPUT		0x20
#define	ARCBIOS_DEVFLAGS_OUTPUT		0x40

typedef struct arc_config
{
	arc_config_class_t	class;
	arc_config_type_t	type;
	arc_dev_flags_t		flags;
	u_int16_t		version;
	u_int16_t		revision;
	u_int32_t		key;
	u_int32_t		affinity_mask;
	u_int32_t		config_data_len;
	u_int32_t		id_len;
	int32_t			id;
} arc_config_t;

typedef struct arc_config64
{
	arc_config_class_t	class;
	arc_config_type_t	type;
	arc_dev_flags_t		flags;
	u_int16_t		version;
	u_int16_t		revision;
	u_int64_t		key;
	u_int64_t		affinity_mask;
	u_int64_t		config_data_len;
	u_int64_t		id_len;
	int64_t			id;
} arc_config64_t;

typedef enum arc_status
{
	arc_ESUCCESS,			/* Success                   */
	arc_E2BIG,			/* Arg list too long         */
	arc_EACCES,			/* No such file or directory */
	arc_EAGAIN,			/* Try again                 */
	arc_EBADF,			/* Bad file number           */
	arc_EBUSY,			/* Device or resource busy   */
	arc_EFAULT,			/* Bad address               */
	arc_EINVAL,			/* Invalid argument          */
	arc_EIO,			/* I/O error                 */
	arc_EISDIR,			/* Is a directory            */
	arc_EMFILE,			/* Too many open files       */
	arc_EMLINK,			/* Too many links            */
	arc_ENAMETOOLONG,		/* File name too long        */
	arc_ENODEV,			/* No such device            */
	arc_ENOENT,			/* No such file or directory */
	arc_ENOEXEC,			/* Exec format error         */
	arc_ENOMEM,			/* Out of memory             */
	arc_ENOSPC,			/* No space left on device   */
	arc_ENOTDIR,			/* Not a directory           */
	arc_ENOTTY,			/* Not a typewriter          */
	arc_ENXIO,			/* No such device or address */
	arc_EROFS,			/* Read-only file system     */
} arc_status_t;

/*
 *	Oops! Arc systems and SGI's have different order of types.
 */
#ifdef __arc__
typedef enum {
	ExceptionBlock,		SystemParameterBlock,	FreeMemory,
	BadMemory,		LoadedProgram,		FirmwareTemporary,
	FirmwarePermanent,	FreeContigous
} MEMORYTYPE;
#endif

#ifdef __sgi__
typedef enum {
	ExceptionBlock,		SystemParameterBlock,	FreeContigous,
	FreeMemory,		BadMemory,		LoadedProgram,
	FirmwareTemporary,	FirmwarePermanent,
} MEMORYTYPE;
#endif

typedef struct arc_mem {
	MEMORYTYPE	Type;		/* Memory chunk type */
	u_int32_t	BasePage;	/* Page no, first page */
	u_int32_t	PageCount;	/* Number of pages */
} arc_mem_t;

typedef struct arc_mem64 {
	MEMORYTYPE	Type;		/* Memory chunk type */
	u_int64_t	BasePage;	/* Page no, first page */
	u_int64_t	PageCount;	/* Number of pages */
} arc_mem64_t;

typedef caddr_t arc_time_t; /* XXX */

typedef struct arc_dsp_stat {
	u_int16_t	CursorXPosition;
	u_int16_t	CursorYPosition;
	u_int16_t	CursorMaxXPosition;
	u_int16_t	CursorMaxYPosition;
	u_char		ForegroundColor;
	u_char		BackgroundColor;
	u_char		HighIntensity;
	u_char		Underscored;
	u_char		ReverseVideo;
} arc_dsp_stat_t;

typedef caddr_t arc_dirent_t; /* XXX */
typedef u_int32_t arc_open_mode_t; /* XXX */
typedef u_int32_t arc_seek_mode_t; /* XXX */
typedef u_int32_t arc_mount_t; /* XXX */

typedef struct arc_quad {
#ifdef __MIPSEB__
	long	hi;
	u_long	lo;
#else
	u_long	lo;
	long	hi;
#endif
} arc_quad_t;

typedef struct arc_calls
{
	arc_status_t (*load)(		/* Load 1 */
		char *,			/* Image to load */
		u_long,			/* top address */
		u_long *,		/* Entry address */
		u_long *);		/* Low address */

	arc_status_t (*invoke)(		/* Invoke 2 */
		u_long,			/* Entry Address */
		u_long,			/* Stack Address */
		u_long,			/* Argc */
		char **,		/* argv */
		char **);		/* envp */

	arc_status_t (*execute)(	/* Execute 3 */
		char *,			/* Image path */
		u_long,			/* Argc */
		char **,		/* argv */
		char **);		/* envp */

	volatile void (*halt)(void);	/* Halt 4 */

	volatile void (*power_down)(void); /* PowerDown 5 */

	volatile void (*restart)(void);	/* Restart 6 */

	volatile void (*reboot)(void);	/* Reboot 7 */

	volatile void (*enter_interactive_mode)(void); /* EnterInteractiveMode 8 */

	volatile void (*return_from_main)(void); /* ReturnFromMain 9 */

	arc_config_t *(*get_peer)(	/* GetPeer 10 */
		arc_config_t *);	/* Component */

	arc_config_t *(*get_child)(	/* GetChild 11 */
		arc_config_t *);	/* Component */

	arc_config_t *(*get_parent)(	/* GetParent 12 */
		arc_config_t *);	/* Component */

	arc_status_t (*get_config_data)( /* GetConfigurationData 13 */
		caddr_t,		/* Configuration Data */
		arc_config_t *);	/* Component */

	arc_config_t *(*add_child)(	/* AddChild 14 */
		arc_config_t *,		/* Component */
		arc_config_t *);	/* New Component */

	arc_status_t (*delete_component)( /* DeleteComponent 15 */
		arc_config_t *);	/* Component */

	arc_config_t *(*get_component)( /* GetComponent 16 */
		char *);		/* Path */

	arc_status_t (*save_config)(void); /* SaveConfiguration 17 */

	arc_sid_t *(*get_system_id)(void); /* GetSystemId 18 */

	arc_mem_t *(*get_memory_descriptor)( /* GetMemoryDescriptor 19 */
		arc_mem_t *);		/* MemoryDescriptor */

#ifdef __arc__
	void (*signal)(			/* Signal 20 */
		u_int32_t,		/* Signal number */
/**/		caddr_t);		/* Handler */
#else
	void *unused;
#endif

	arc_time_t *(*get_time)(void);	/* GetTime 21 */

	u_long (*get_relative_time)(void); /* GetRelativeTime 22 */

	arc_status_t (*get_dir_entry)(	/* GetDirectoryEntry 23 */
		u_long,			/* FileId */
		arc_dirent_t *,		/* Directory entry */
		u_long,			/* Length */
		u_long *);		/* Count */

	arc_status_t (*open)(		/* Open 24 */
		char *,			/* Path */
		arc_open_mode_t,	/* Open mode */
		u_long *);		/* FileId */

	arc_status_t (*close)(		/* Close 25 */
		u_long);		/* FileId */

	arc_status_t (*read)(		/* Read 26 */
		u_long,			/* FileId */
		caddr_t,		/* Buffer */
		u_long,			/* Length */
		u_long *);		/* Count */

	arc_status_t (*get_read_status)( /* GetReadStatus 27 */
		u_long);		/* FileId */

	arc_status_t (*write)(		/* Write 28 */
		u_long,			/* FileId */
		caddr_t,		/* Buffer */
		u_long,			/* Length */
		u_long *);		/* Count */

	arc_status_t (*seek)(		/* Seek 29 */
		u_long,			/* FileId */
		arc_quad_t *,		/* Offset */
		arc_seek_mode_t);	/* Mode */

	arc_status_t (*mount)(		/* Mount 30 */
		char *,			/* Path */
		arc_mount_t);		/* Operation */

	char *(*getenv)(			/* GetEnvironmentVariable 31 */
		char *);		/* Variable */

	arc_status_t (*putenv)(		/* SetEnvironmentVariable 32 */
		char *,			/* Variable */
		char *);		/* Value */

	arc_status_t (*get_file_info)(void);	/* GetFileInformation 33 */

	arc_status_t (*set_file_info)(void);	/* SetFileInformation 34 */

	void (*flush_all_caches)(void);	/* FlushAllCaches 35 */

#ifdef __arc__
	arc_status_t (*test_unicode)(	/* TestUnicodeCharacter 36 */
		u_int32_t,		/* FileId */
		u_int16_t);		/* UnicodeCharacter */

	arc_dsp_stat_t *(*get_display_status)( /* GetDisplayStatus 37 */
		u_int32_t);		/* FileId */
#endif
} arc_calls_t;

#define ARC_PARAM_BLK_MAGIC	0x53435241
#define ARC_PARAM_BLK_MAGIC_BUG	0x41524353	/* This is wrong... but req */

typedef struct arc_param_blk_32
{
	u_int32_t	magic;		/* Magic Number */
	u_int32_t	length;		/* Length of parameter block */
	u_int16_t	version;	/* ?? */
	u_int16_t	revision;	/* ?? */
	u_int32_t	restart_block;	/* ?? */
	u_int32_t	debug_block;	/* Debugging info -- unused */
	u_int32_t	general_exp_vect; /* ?? */
	u_int32_t	tlb_miss_exp_vect; /* ?? */
	u_int32_t	firmware_length; /* Size of Firmware jumptable in bytes */
	u_int32_t	*firmware_vect;	/* Firmware jumptable */
	u_int32_t	vendor_length;	/* Size of Vendor specific jumptable */
	u_int32_t	vendor_vect;	/* Vendor specific jumptable */
	u_int32_t	adapter_count;	/* ?? */
	u_int32_t	adapter0_type;	/* ?? */
	u_int32_t	adapter0_length; /* ?? */
	u_int32_t	adapter0_vect;	/* ?? */
} arc_param_blk_32_t;

typedef struct arc_param_blk_64
{
	u_int64_t	magic;		/* Magic Number */
	u_int64_t	length;		/* Length of parameter block */
	u_int16_t	version;	/* ?? */
	u_int16_t	revision;	/* ?? */
	u_int64_t	restart_block;	/* ?? */
	u_int64_t	debug_block;	/* Debugging info -- unused */
	u_int64_t	general_exp_vect; /* ?? */
	u_int64_t	tlb_miss_exp_vect; /* ?? */
	u_int64_t	firmware_length; /* Size of Firmware jumptable in bytes */
	u_int64_t	*firmware_vect;	/* Firmware jumptable */
	u_int64_t	vendor_length;	/* Size of Vendor specific jumptable */
	u_int64_t	vendor_vect;	/* Vendor specific jumptable */
	u_int64_t	adapter_count;	/* ?? */
	u_int64_t	adapter0_type;	/* ?? */
	u_int64_t	adapter0_length; /* ?? */
	u_int64_t	adapter0_vect;	/* ?? */
} arc_param_blk_64_t;

#ifdef _LP64
#define ArcBiosBase32	((arc_param_blk_32_t *) 0xffffffff80001000)
#define ArcBiosBase64	((arc_param_blk_64_t *) 0xffffffff80001000)
#else
#define ArcBiosBase32	((arc_param_blk_32_t *) 0x80001000)
#define ArcBiosBase64	((arc_param_blk_64_t *) 0x80001000)
#endif
#define ArcBios (ArcBiosBase->firmware_vect)

#define	ARCBIOS_PAGE_SIZE	4096

extern int bios_is_32bit;
extern char bios_enaddr[20];
extern char bios_console[30];
extern char bios_graphics[6];
extern char bios_keyboard[6];

int  bios_getchar(void);
void bios_putchar(char);
void bios_putstring(char *);
void bios_printf(const char *, ...);
void bios_ident(void);
void bios_display_info(int *, int *, int *, int *);

/*
 * Direct ARC-BIOS calls.
 */
long Bios_Load(char *, u_long, u_long, u_long *);
long Bios_Invoke(u_long, u_long, u_long, char **, char **);
long Bios_Execute(char *, u_long, char **, char **);
void Bios_Halt(void);
void Bios_PowerDown(void);
void Bios_Restart(void);
void Bios_Reboot(void);
void Bios_EnterInteractiveMode(void);
long Bios_GetPeer(void *);
arc_config_t *Bios_GetChild(void *);
long Bios_GetParent(void *);
long Bios_GetConfigurationData(void *, void *);
long Bios_AddChild(void *, void *);
long Bios_DeleteComponent(void *);
long Bios_GetComponent(char *);
long Bios_SaveConfiguration(void);
arc_sid_t *Bios_GetSystemId(void);
arc_mem_t *Bios_GetMemoryDescriptor(void *);
long Bios_GetTime(void);
long Bios_GetRelativeTime(void);
long Bios_GetDirectoryEntry(u_long, void *, u_long, u_long *);
long Bios_Open(char *, int, long *);
long Bios_Close(long);
long Bios_Read(long, char *, long, long *);
long Bios_GetReadStatus(u_long);
long Bios_Write(long, char *, long, long *);
long Bios_Seek(long, arc_quad_t *, int);
long Bios_Mount(char *, void *);
char *Bios_GetEnvironmentVariable(const char *);
long Bios_SetEnvironmentVariable(char *, char *);
long Bios_GetFileInformation(u_long, u_long, u_long);
long Bios_SetFileInformation(u_long, u_long, u_long);
void Bios_FlushAllCaches(void);
long Bios_TestUnicodeCharacter(u_long, u_int16_t);
arc_dsp_stat_t *Bios_GetDisplayStatus(u_long);

