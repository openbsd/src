/*	$NetBSD: main.c,v 1.5 1994/10/27 04:21:17 cgd Exp $	*/

/*
 * source code in this file is from:
 * 386BSD boot blocks by Julian Elischer (julian@tfs.com)
 * 386BSD Adaptec 1542 SCSI boot blocks by Pace Willisson (pace@blitz.com)
 *
 * Mach Operating System
 * Copyright (c) 1992, 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

/*
  Copyright 1988, 1989, 1990, 1991, 1992 
   by Intel Corporation, Santa Clara, California.

                All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Intel
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

INTEL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL INTEL BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/


#include "config.h"
#include "nbtypes.h"
#include "assert.h"
#include "param.h"
#include "packet.h"
#include "ether.h"
#include "inet.h"
#include "arp.h"
#include "tftp.h"

#include "proto.h"


u_long work_area_org = RELOC;
u_long kernel_size;
u_short real_cs;
u_long howto = 0;
u_char vendor_area[64];

enum LoadErr {
  load_success,
  load_no_answer,
  load_bad_format,
  load_bad_size,
};

static inline enum LoadErr
LoadProgFromServer(ipaddr_t server, ipaddr_t gateway, char *file_name) {
  /* TBD - fail (or at least warn) if we don't get the entire file */
  u_long start_addr;
  u_long boot_area_org;
  u_long tmp_reloc_org;
  u_long kern_copy_org;
  u_long kern_copy_p;
  struct exec head;
  u_long boot_argv[4];
#if 1 /* ndef USE_BUFFER */
  u_char tmpbuf[4096]; /* we need to load the first 4k here */
#endif

  SetTftpParms(server, gateway, file_name);

  /* TBD - heuristics about what we are loading - load linux, ... also */

  if (Read(&head, sizeof(head)) != sizeof(head))
    return load_bad_size;

  if (N_GETMAGIC(head) != ZMAGIC) {
    printf("Invalid format!\n");
    return load_bad_format;
  }

  /* Load/tftp the executable to a temporary area beyond 640k - after
     successful load, copy to correct area. If load fails or is interrrupted,
     we can recover gracefully
     */

  start_addr = (u_long)head.a_entry;
  boot_area_org = start_addr & 0x00f00000; /* some MEG boundary */
  tmp_reloc_org = 0x00100000;
#ifdef USE_BUFFER
  kern_copy_org = tmp_reloc_org + WORK_AREA_SIZE; /* leave free for relocation */
#else
  kern_copy_org = boot_area_org; /* leave free for relocation */
#endif
  kern_copy_p = kern_copy_org;
  kernel_size = head.a_text + head.a_data + head.a_bss;

  printf("Loading ");
  IpPrintAddr(server);
  printf(":%s @ 0x%x\n", file_name, boot_area_org);
#ifndef USE_BUFFER
  if(boot_area_org < work_area_org) {
    if((boot_area_org + head.a_text + head.a_data) > work_area_org) {
      printf("kernel will not fit below loader\n");
      return load_bad_size;
    }
    if((boot_area_org + head.a_text + head.a_data + head.a_bss) > 0xa0000) {
      printf("kernel too big, won't fit in 640K with bss\n");
      printf("Only hope is to link the kernel for > 1MB\n");
      return load_bad_size;
    }
    if(boot_area_org + kernel_size > work_area_org) {
      printf("loader overlaps bss, kernel must bzero\n");
    }
  }
#else
  if(boot_area_org + kernel_size > 0xa0000) {
    printf("kernel too big, won't fit in 640K with bss\n");
    printf("Only hope is to link the kernel for > 1MB\n");
    return load_bad_size;
  }
  /* check if too large for tmp buffer (TBD) */
#endif
  printf("text=0x%x", head.a_text);

#if 01
  /* skip to first 4k boundry */
  Read(tmpbuf, 4096-sizeof(head));
#endif

#if 0 /* ndef USE_BUFFER */
  /********************************************************/
  /* LOAD THE TEXT SEGMENT				*/
  /* don't clobber the first 4k yet (BIOS NEEDS IT) 	*/
  /********************************************************/
  Read(tmpbuf, sizeof(tmpbuf));
  kern_copy_p += sizeof(tmpbuf);
  PhysRead(kern_copy_p, head.a_text - sizeof(tmpbuf));
  kern_copy_p += head.a_text - sizeof(tmpbuf);
#else
  /********************************************************/
  /* LOAD THE TEXT SEGMENT				*/
  /********************************************************/
  PhysRead(kern_copy_p, head.a_text);
  kern_copy_p += head.a_text;
#endif

  /********************************************************/
  /* Load the Initialised data after the text		*/
  /********************************************************/
/* TBD - this is bogus - file system oriented */
#define CLSIZE 1
#define NBPG 4096            /* bytes/page */
#define CLOFSET (CLSIZE*NBPG-1) /* for clusters, like PGOFSET */

  while (kern_copy_p & CLOFSET)
    *(char *)kern_copy_p++ = 0;

  printf(" data=0x%x", head.a_data);
  PhysRead(kern_copy_p, head.a_data);
  kern_copy_p += head.a_data;

  /********************************************************/
  /* Skip over the uninitialised data			*/
  /* (but clear it)					*/
  /********************************************************/
  printf(" bss=0x%x", head.a_bss);
  if (kern_copy_p < RELOC &&
     (kern_copy_p + head.a_bss) > RELOC) {
    PhysBzero(kern_copy_p, RELOC-(u_int)kern_copy_p);
  } else {
    PhysBzero(kern_copy_p, head.a_bss);
  }

#ifdef LOADSYMS /* not yet, haven't worked this out yet */
  if (kern_copy_p > 0x100000) {
    /********************************************************/
    /*copy in the symbol header				*/
    /********************************************************/
    PhysBcopy(&head.a_syms, kern_copy_p, sizeof(head.a_syms));
    kern_copy_p += sizeof(head.a_syms);

    /********************************************************/
    /* READ in the symbol table				*/
    /********************************************************/
    printf(" symbols=[+0x%x", head.a_syms);
    Read(kern_copy_p, head.a_syms);
    kern_copy_p += head.a_syms;

    /********************************************************/
    /* Followed by the next integer (another header)	*/
    /* more debug symbols?					*/
    /********************************************************/
    read(&i, sizeof(u_int));
    PhysBcopy(&i, kern_copy_p, sizeof(u_int));
    i -= sizeof(u_int);
    kern_copy_p += sizeof(u_int);

    /********************************************************/
    /* and that many bytes of (debug symbols?)		*/
    /********************************************************/
    printf("+0x%x]", i);
    Read(kern_copy_p, i);
    kern_copy_p += i;
  }
#endif	LOADSYMS

  /********************************************************/
  /* and note the end address of all this			*/
  /********************************************************/

  printf(" total=0x%x",kern_copy_p - kern_copy_org);

  boot_argv[0] = 0;
  boot_argv[1] = howto;
  boot_argv[2] = 0;
  boot_argv[3] = 0;

/* TBD - place vendor_area on stack */

  printf(" entry point=0x%x\n" ,((u_int)start_addr) & 0xffffff);

  if (howto) {
    static char *rb_option_name[9] = {
      "askname",
      "single",
      "nosync",
      "halt",
      "initname",
      "dfltroot",
      "kdb",
      "rdonly",
      "dump",
      };
    int i;
    printf("Starting kernel with options (0x%x): ", howto);
    for (i=0; i<9; i++) {
      if (howto & (1<<i)) {
	printf("%s ", rb_option_name[i]);
      }
    }
    printf("\n");
  }

#if 0 /* ndef USE_BUFFER */
  PhysBcopy(tmpbuf, boot_area_org, sizeof(tmpbuf));
#endif

  StartProg(start_addr & 0xffffff, boot_argv);

  return load_success; /* hah! */
}

int GetHex(int old) {
  int r, c, ch;
  r = 0;
  ch = 0;
  while((c=getchar()) != '\n') {
    ch = 1;
    if (c>='0' && c<='9')
      r = r*16 + (c-'0');
    else if (c>='a' && c<='f')
      r = r*16 + (c+10-'a');
  }
  if (ch)
    return r;
  else
    return old;
}

static char
ToHex(int n) {
  n &= 0x0F;
  return n >= 10 ? n - 10 + 'A' : n + '0';
}

static char name_set[][9] = {
  "xxxxxxxx",
  "default",
};

static char *ext_set[] = {
  ".bsd",
  ".bsd.old",
  ".vmunix",
  ".vmunix.old",
};

static ipaddr_t server_set[2] = {
  0,
  IP_BCASTADDR,
};

static inline void
TryToLoadSomething(void) {
  int nserver;
  for (nserver=0; nserver<nelt(server_set); nserver++) {
    int nname;
    char file_name[MAX_FILE_NAME_LEN+1];
    file_name[0] = '\0';
    if (GetIpAddress(&server_set[nserver], &ip_myaddr, &ip_gateway, file_name)) {
      if (*file_name) {
	LoadProgFromServer(server_set[nserver], ip_gateway, file_name);
      }
      else {
	/* no file name supplied from server, synthesize one */
	inetaddr_t ip;
	ip.a = ip_myaddr;
	name_set[0][0] = ToHex(ip.s.a0 >> 4);
	name_set[0][1] = ToHex(ip.s.a0);
	name_set[0][2] = ToHex(ip.s.a1 >> 4);
	name_set[0][3] = ToHex(ip.s.a1);
	name_set[0][4] = ToHex(ip.s.a2 >> 4);
	name_set[0][5] = ToHex(ip.s.a2);
	name_set[0][6] = ToHex(ip.s.a3 >> 4);
	name_set[0][7] = ToHex(ip.s.a3);
	name_set[0][8] = 0;
	for (nname=0; nname<nelt(name_set); nname++) {
	  int next;
	  for (next=0; next<nelt(ext_set); next++) {
	    strncpy(file_name, name_set[nname], MAX_FILE_NAME_LEN);
	    strncat(file_name, ext_set[next], MAX_FILE_NAME_LEN-strlen(file_name));
	    LoadProgFromServer(server_set[nserver], ip_gateway, file_name);
	  }
	}
      }
    }
  }
}


static char *
DecimalToByte(char *s, u_char *n) {
  for (*n = 0; *s >= '0' && *s <= '9'; s++)
    *n = (*n * 10) + *s - '0';
  return s;
}

static ipaddr_t
IpConvertAddr(char *p) {
  inetaddr_t addr;

  if (p == (char *)0 || *p == '\0')
    return IP_ANYADDR;
  p = DecimalToByte(p, &addr.s.a0);
  if (*p == '\0' || *p++ != '.')
    return IP_ANYADDR;
  p = DecimalToByte(p, &addr.s.a1);
  if (*p == '\0' || *p++ != '.')
    return IP_ANYADDR;
  p = DecimalToByte(p, &addr.s.a2);
  if (*p == '\0' || *p++ != '.')
    return IP_ANYADDR;
  p = DecimalToByte(p, &addr.s.a3);
  if (*p != '\0')
    return IP_ANYADDR;
  return addr.a;
}

static int
GetLine(char **argv, int argvsize) {
  char *p, ch;
  static char line[128];
  int argc;

  /*
   * Read command line, implement some simple editing features
   */
  p = line;
  while ((ch = getchar()) != '\r' && ch != '\n' && (p-line) < sizeof(line)) {
    if (ch == '\b') {
      if (p > line) {
	p--;
	printf(" \b");
      }
    } else
      *p++ = ch;
  }
  *p = '\0';

  /*
   * Break command line up into an argument vector
   */
  argc = 0;
  for (p = line; *p == ' ' || *p == '\t'; p++)
    /* skip white spaces */;
  while (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\0') {
    if (argc > argvsize) break;
    argv[(argc)++] = p;
    for (; *p != ' ' && *p != '\t' && *p != '\n' && *p != '\0'; p++)
      /* skip word */;
    if (*p != '\0') *p++ ='\0';
    for (; *p == ' ' || *p == '\t'; p++)
      /* skip white spaces */;
  }
  argv[(argc)] = (char *)0;
  return argc;
}

enum cmd_token_type {
  CMD_GO,
  CMD_HELP,
  CMD_RESET,
  CMD_AUTO,
  CMD_WHO,
  CMD_REBOOT,
  CMD_DISKBOOT,
  CMD_ADDRESS,
  CMD_GATEWAY,
  CMD_TFTP,
  CMD_DEBUG,
  CMD_SINGLE,
  CMD_FILE,
  CMD_SERVER,
};

struct commands {
  enum cmd_token_type cmd_token;
  char *cmd_name;
  char *cmd_help;
} commands[] = {
  {CMD_GO, 	"go",		"                       start loaded binary"},
  {CMD_HELP,	"help",		"                     this help message"},
  {CMD_RESET,	"reset",	"                    reset ethernet board"},
  {CMD_AUTO,	"auto",		"                     continue with auto boot"},
  {CMD_WHO,	"whoami",	"                       get IP address"},
  {CMD_REBOOT,	"reboot",	"                   hard reboot"},
  {CMD_DISKBOOT,	"diskboot",	"                 soft reboot (from disk)"},
  {CMD_ADDRESS,  "address",	"[<addr>]          set IP address"},
  {CMD_GATEWAY,  "gateway",	"[<addr>]          set IP gateway"},
  {CMD_SERVER, "server", "[<addr>]           set server IP address"},
  {CMD_FILE, "file", "[<name>]             set boot file name"},
  {CMD_TFTP,	"tftp",		"                     TFTP download"},
  {CMD_DEBUG, "debug", "                    enter kernel debugger at boot"},
  {CMD_SINGLE, "single", "                   enter single user at boot"},
};

#define ARGVECSIZE 100

/*
 * A very simple and terse monitor
 */
static void
Monitor(void) {
  char *argv[ARGVECSIZE];
  static char file_name[MAX_FILE_NAME_LEN+1] = "default.bsd386";
  int loaded, argc;
  int i;
  int token;
  static ipaddr_t ip_servaddr = IP_ANYADDR;

  loaded = 0;
  printf("\n"
#ifdef USE_BOOTP
	 "BOOTP/"
#endif
#ifdef USE_RARP
	 "RARP/"
#endif
	 "TFTP monitor mode\n");
  for (;;) {
    char filename[MAX_FILE_NAME_LEN+1];
    ipaddr_t gateway;
    printf("ethernet boot monitor: ");
    if ((argc = GetLine(argv, ARGVECSIZE)) > 0) {

      for (token = -1, i = 0; i < nelt(commands); i++) {
	if (strcmp(argv[0], commands[i].cmd_name) == 0) {
	  token = commands[i].cmd_token;
	  break;
	}
      }
      switch (token) {
      case CMD_HELP:
	for (i = 0; i < nelt(commands); i++)
	  printf("%s %s\n", commands[i].cmd_name, commands[i].cmd_help);
	break;
      case CMD_RESET:
	PktInit();
	EtherReset();
	break;
      case CMD_REBOOT:
	EtherStop();
	ResetCpu();
	break;
      case CMD_DISKBOOT:
	EtherStop();
	exit(0);
	break;
      case CMD_AUTO:
	return;
      case CMD_WHO:
	(void) GetIpAddress(&ip_servaddr, &ip_myaddr, &gateway, filename);
	break;
      case CMD_ADDRESS:
	if (argc != 2) {
	  printf("My IP address is ");
	  IpPrintAddr(ip_myaddr);
	  printf("\n");
	} else
	  ip_myaddr = IpConvertAddr(argv[1]);
	break;
      case CMD_SERVER:
	if (argc != 2) {
	  printf("Server's IP address is ");
	  IpPrintAddr(ip_servaddr);
	  printf("\n");
	} else
	  ip_servaddr = IpConvertAddr(argv[1]);
	break;
      case CMD_FILE:
	if (argc != 2) {
	  printf("File name is \"%s\"\n", file_name);
	} else
	  strncpy(file_name, argv[1], MAX_FILE_NAME_LEN);
	break;
      case CMD_GATEWAY:
	if (argc != 2) {
	  printf("Gateway IP address is ");
	  IpPrintAddr(ip_gateway);
	  printf("\n");
	} else
	  ip_gateway = IpConvertAddr(argv[1]);
	break;
      case CMD_DEBUG:
	howto ^= RB_KDB;
	break;
      case CMD_SINGLE:
	howto ^= RB_SINGLE;
	break;
      case CMD_TFTP:
	if (ip_myaddr == IP_ANYADDR) {
	  printf("This machine's IP address must be set first.\n");
	  goto complain;
	}
	loaded = LoadProgFromServer(ip_servaddr, ip_gateway, file_name);
	printf("File could not be loaded, giving up.\n");
	break;
      default:
	goto complain;
      }
    } else
    complain:
      printf("Invalid or incorrect command. Type \"help\" for help.\n");
  }
}

static jmp_buf jmp_env;

void
HandleKbdAttn(void) {
  if (IsKbdCharReady())
    if (getc() == 0x1b) {
      EtherReset();
      PktInit();
      longjmp(jmp_env, 1);
    }
}

int time_zero;

void
main(void) {

  extern char edata[], end[];
  char volatile * volatile p;

  /* clear bss */
  for (p = edata; p < end; p++)
    *p = 0;

  printf(
#ifdef USE_BOOTP
	 "BOOTP/"
#endif
#ifdef USE_RARP
	 "RARP/"
#endif
	 "TFTP bootstrap loader @0x%x: %d/%d k of memory. ^] for attn.\n",
	 work_area_org,
	 GetMemSize(0),
	 GetMemSize(1));

  gateA20();

  PktInit();
  if (!EtherInit()) {
    printf("No ethernet board found\n");
    exit(1);
  }
  srand((time_zero=timer()) ^ eth_myaddr[5]);

  printf("Ethernet address is ");
  EtherPrintAddr(eth_myaddr);
  printf("\n");

  for (;;) {
    if (setjmp(jmp_env))
      Monitor();
    else {
      TryToLoadSomething();
    }
  }
}

#ifdef __GNUC__
void
__main(void) {
}
#endif
