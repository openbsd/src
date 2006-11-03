/* $OpenBSD: acpiscan.c,v 1.1 2006/11/03 19:33:56 marco Exp $ */
/*
 * Copyright (c) 2006 Jordan Hargrave <jordan@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "oslib.h"

static const char  *savename;

#define ACPI_LAPIC    0x00
#define ACPI_IOAPIC   0x01
#define ACPI_INTSRC   0x02
#define ACPI_NMISRC   0x03
#define ACPI_LAPICNMI 0x04

#define MPC_CPU  0x00
#define MPC_BUS  0x01
#define MPC_APIC 0x02
#define MPC_INT  0x03
#define MPC_LINT 0x04

/* MPBIOS Structures */
typedef struct _mps_root
{
	char        mph_signature[4];  /* _MP_ */
	uint32_t    mph_physaddr;
	uint8_t     mph_length;
	uint8_t     mph_spec;
	uint8_t     mph_cksum;
	uint8_t     mph_feature[5];
} PACKED mps_root;

typedef struct _mps_header
{
	char        mpc_signature[4];
	uint16_t    mpc_length;
	uint8_t     mpc_spec;   
	uint8_t     mpc_cksum;
	char        mpc_oem[8];
	char        mpc_product[12];
	uint32_t    mpc_oemptr;
	uint16_t    mpc_oemsize;
	uint16_t    mpc_oemcount;
	uint32_t    mpc_lapic;
	uint32_t    reserved;
} PACKED mps_header;

typedef struct _mps_cpu
{
	uint8_t    type;
	uint8_t    apicid;
	uint8_t    apicver;
	uint8_t    cpuflag;
	uint32_t   cpufeature;
	uint32_t   featureflag;
	uint32_t   reserved[2];
} PACKED mps_cpu;

typedef struct _mps_bus
{
	uint8_t    type;
	uint8_t    busid;
	char       bustype[6];
} PACKED mps_bus;

typedef struct _mps_ioapic
{
	uint8_t    type;
	uint8_t    apicid;
	uint8_t    apicver;
	uint8_t    flags;
	uint32_t   apicaddr;
} PACKED mps_ioapic;

typedef struct _mps_intsrc
{
	uint8_t    type;
	uint8_t    irqtype;
	uint16_t   irqflag;
	uint8_t    srcbus;
	uint8_t    srcbusirq;
	uint8_t    dstapic;
	uint8_t    dstirq;
} PACKED mps_intsrc;

typedef struct _mps_lintsrc
{
	uint8_t    type;
	uint8_t    irqtype;
	uint16_t   irqflag;
	uint8_t    srcbus;
	uint8_t    srcbusirq;
	uint8_t    dstapic;
	uint8_t    dstapiclint;
} PACKED mps_lintsrc;

typedef union _mps_entry
{
	uint8_t       type;
	mps_cpu       m_cpu;
	mps_bus       m_bus;
	mps_ioapic    m_ioapic;
	mps_intsrc    m_int;
	mps_lintsrc   m_lint;
} PACKED mps_entry;

/* ACPI Structures */
typedef struct _acpi_table_rsdp
{
	char       signature[8];
	uint8_t    checksum;
	char       oem_id[6];
	uint8_t    revision;
	uint32_t   rsdt_address;
	uint32_t   length;
	uint64_t   xsdt_address;
	uint8_t    xchecksum;
	uint8_t    reserved[3];
} PACKED acpi_table_rsdp;

typedef struct _acpi_table_header
{
	uint8_t    sig[4];
	uint32_t   length;
	uint8_t    rev;
	uint8_t    checksum;
	uint8_t    oem[6];
	uint8_t    oem_table[8];
	uint32_t   oem_rev;
	char       asl_compiler_id[4];
	uint32_t   asl_compiler_rev;
} PACKED acpi_table_header;

typedef struct _acpi_table_madt
{
	uint32_t   local_apic_addr;
	uint32_t   compat;
} PACKED acpi_table_madt;

typedef struct _acpi_table_entry_header
{
	uint8_t    type;
	uint8_t    length;
} PACKED acpi_table_entry_header;

typedef struct _acpi_facp
{
	uint32_t   facs;
	uint32_t   dsdt;
	uint8_t    model;
	uint8_t    resvd;
	uint16_t   sciint;
	uint32_t   smicmd;
	uint8_t    enable;
	uint8_t    disable;
	uint8_t    s4bios;
	uint8_t    resvd2;
	uint32_t   pm1a_evt;
	uint32_t   pm1b_evt;
	uint32_t   pm1a_cnt;
	uint32_t   pm1b_cnt;
	uint32_t   pm2_cnt;
	uint32_t   pm_tmr;
	uint32_t   gpe0;
	uint32_t   gpe1;
	uint8_t    pm1_evt_len;
	uint8_t    pm1_cnt_len;
	uint8_t    pm2_cnt_len;
	uint8_t    pm_tm_len;
	uint8_t    gpe0_len;
	uint8_t    gpe1_len;
	uint8_t    gpe1_base;
	uint8_t    resvd3;
	uint16_t   lvl2;
	uint16_t   lvl3;
	uint16_t   size;
	uint16_t   stride;
	uint8_t    offset;
	uint8_t    width;
	uint8_t    day;
	uint8_t    mon;
	uint8_t    century;
	uint16_t   arch;
	uint8_t    resvd4;
	uint32_t   flags;
	uint8_t    reset_reg[12];
	uint8_t    reset_val;
	uint8_t    resvd5[3];
	uint64_t   x_facs;
	uint64_t   x_dsdt;
} PACKED acpi_facp;

typedef struct _acpi_table_lapic
{
	uint8_t   acpi_id;
	uint8_t   apic_id;
	uint32_t  enabled;
} PACKED acpi_table_lapic;

typedef struct _acpi_table_ioapic
{
	uint8_t   id;
	uint8_t   resvd;
	uint32_t  address;
	uint32_t  irq_base;
} PACKED acpi_table_ioapic;

typedef struct _acpi_table_int_src_ovr
{
	uint8_t   bus;
	uint8_t   bus_irq;
	uint8_t   global_irq;
	uint16_t  flags;
} PACKED acpi_table_int_src_ovr;

typedef struct _acpi_table_lapic_nmi
{
	uint8_t   acpi_id;
	uint16_t  flags;
	uint8_t   lint;
} PACKED acpi_table_lapic_nmi;

typedef struct _acpi_table_entry
{
	acpi_table_entry_header   hdr;
	union {
		acpi_table_lapic        m_lapic;
		acpi_table_ioapic       m_ioapic;
		acpi_table_int_src_ovr  m_intsrc;
		//acpi_table_nmi_src      m_nmisrc;
		acpi_table_lapic_nmi    m_lapicnmi;
	} PACKED x;
} PACKED acpi_table_entry;

#ifdef MSDOS
#pragma pack()
#endif

/* Show Table Entry */
void show_acpitab(uint64_t addr);

/* Return a string of n bytes */
const char FAR
*zstr(const char FAR *src, int n)
{
	static char tmp[32];

	if (n >= sizeof(tmp)) {
		n = sizeof(tmp)-1;
	}
	strncpy(tmp, src, n);
	tmp[n] = 0;
	return tmp;
}

void
dump_facp(int rev, long len, void *buf)
{
	acpi_facp *afp = (acpi_facp *)buf;
	uint8_t buf2[128];

	if (rev == 3) {
		printf(" XFACS   : %llx\n", afp->x_facs);
		printf(" XDSDT   : %llx\n", afp->x_dsdt);
	}
	else {
		printf(" FACS    : %lx\n", afp->facs);
		printf(" DSDT    : %lx\n", afp->dsdt);
	}
	printf(" Model   : %x\n", afp->model);
	printf(" SCI Int : %x\n", afp->sciint);
	printf(" SMI Cmd : %x\n", afp->smicmd);
	printf(" S4 BIOS : %x\n", afp->s4bios);
	printf(" enable  : %x\n", afp->enable);
	printf(" disable : %x\n", afp->disable);
	printf(" PM1A    : %.8x/%.8x\n", afp->pm1a_evt, afp->pm1a_cnt);
	printf(" PM1B    : %.8x/%.8x\n", afp->pm1b_evt, afp->pm1b_cnt);
	printf(" PM2     : %x\n", afp->pm2_cnt);

	if (physmemcpy(buf2, afp->facs, 128) == 0) {
		printf("------ (facs) ------\n");
		dump(buf2, 128);
	}

	/* Show DSDT */
	if (rev == 3) {
		show_acpitab(afp->x_dsdt);
	}
	else {
		show_acpitab(afp->dsdt);
	}
}

void dump_madt(int rev, long len, void *buf)
{
	acpi_table_madt  *pmad = (acpi_table_madt *)buf;
	acpi_table_entry *phdr = (acpi_table_entry *)&pmad[1];

	printf("------------( madt ) ------------\n");
	printf(" Local APIC: %lx\n", (long)pmad->local_apic_addr);
	printf(" Compat    : %lx\n", (long)pmad->compat);
	len -= sizeof(acpi_table_madt);

	while(len > 0) {
		if (phdr->hdr.length == 0) {
			printf("Invalid entry\n");
			break;
		}
		len -= phdr->hdr.length;
		printf("  type  : %x  length: %x", phdr->hdr.type, phdr->hdr.length);
		switch(phdr->hdr.type) {
			case ACPI_LAPIC: 
				printf("  (lapic)\tacpi_id:%.2x id:%.2x en:%.2x\n",
						phdr->x.m_lapic.acpi_id, phdr->x.m_lapic.apic_id, phdr->x.m_lapic.enabled);
				break;
			case ACPI_IOAPIC: 
				printf("  (ioapic)\tioapic :%.2x addr:%.8x irq:%.4x\n",
						phdr->x.m_ioapic.id, phdr->x.m_ioapic.address, phdr->x.m_ioapic.irq_base);
				break;
			case ACPI_INTSRC: 
				printf("  (int_src_ovr)\tbus:%.2x busirq:%.2x globalirq:%.2x flags:%.4x\n",
						phdr->x.m_intsrc.bus, phdr->x.m_intsrc.bus_irq, phdr->x.m_intsrc.global_irq, 
						phdr->x.m_intsrc.flags);
				break;
			case ACPI_NMISRC: 
				printf("  nmi_src\n"); 
				break;
			case ACPI_LAPICNMI: 
				printf("  (lapic_nmi)\tacpi_id:%.2x flags:%.4x lint:%.2x\n",
						phdr->x.m_lapicnmi.acpi_id, phdr->x.m_lapicnmi.flags, phdr->x.m_lapicnmi.lint);
				break;
			default:
				printf("  unknown : %x\n", phdr->hdr.type); 
				break;
		}
		phdr = (acpi_table_entry *)((uint8_t *)phdr + phdr->hdr.length);
	}
}

void
dump_xsdt(int rev, uint32_t len, void *buf)
{
	uint64_t *pb = (uint64_t *)buf;

	len /= sizeof(*pb);
	while(len--) {
		show_acpitab(*(pb++));
	}
}

void
dump_rsdt(int rev, uint32_t len, void *buf)
{
	uint32_t *pb = (uint32_t *)buf;

	len /= sizeof(*pb);
	while(len--) {
		show_acpitab(*(pb++));
	}
}

void
dump_dsdt(int rev, uint32_t len, void *buf)
{
}

/*========================================================================*
 * Parse ACPI Table contents
 *========================================================================*/
void
parse_acpitab(acpi_table_header *atab, uint32_t physaddr, uint32_t len)
{
	void *buf;
	FILE *fp;
	char  name[64];
	static int tid;

	if ((buf = malloc(len)) == NULL) {
		return;
	}
	physmemcpy(buf, physaddr, len);
	dump(buf, len);

	/* Save Table data */
	if (savename != NULL) {
		snprintf(name, sizeof(name), "%s.%s.%d", savename, zstr(atab->sig, 4), tid++);
		if ((fp = fopen(name, "wb")) != NULL) {
			fwrite(atab, sizeof(*atab), 1, fp);
			fwrite(buf, len, 1, fp);
			fclose(fp);
		}
	}
	if (!strncmp(atab->sig, "SSDT", 4)) dump_dsdt(atab->rev, len, buf);
	if (!strncmp(atab->sig, "DSDT", 4)) dump_dsdt(atab->rev, len, buf);
	if (!strncmp(atab->sig, "XSDT", 4)) dump_xsdt(atab->rev, len, buf);
	if (!strncmp(atab->sig, "RSDT", 4)) dump_rsdt(atab->rev, len, buf);
	if (!strncmp(atab->sig, "APIC", 4)) dump_madt(atab->rev, len, buf);
	if (!strncmp(atab->sig, "FACP", 4)) dump_facp(atab->rev, len, buf);
	free(buf);
}

/*========================================================================*
 * Display ACPI Table header
 *========================================================================*/
void
show_acpitab(uint64_t addr)
{
	acpi_table_header atab;

	if (physmemcpy(&atab, addr, sizeof(atab)) != 0) {
		return;
	}
	printf("--------------------------------------\n");
	printf("%s @ 0x%lx\n", zstr(atab.sig, 4), (long)addr);
	printf("  length : %x\n", atab.length);
	printf("  rev    : %x\n", atab.rev);
	printf("  cksum  : %x\n", atab.checksum);
	printf("  oem    : %s\n", zstr(atab.oem, 6));
	printf("  oemtab : %s\n", zstr(atab.oem_table, 8));
	printf("  oem_rev: %x\n", atab.oem_rev);
	printf("  asl id : %s\n", zstr(atab.asl_compiler_id, 4));
	printf("  asl rev: %x\n", atab.asl_compiler_rev);

	parse_acpitab(&atab, addr + sizeof(atab), atab.length - sizeof(atab));
}

/*===============================================================
 * Display ACPI Table root header
 *===============================================================*/
void
show_acpi(uint32_t addr)
{
	acpi_table_rsdp rsdp;

	if (physmemcpy(&rsdp, addr, sizeof(rsdp)) != 0) {
		return;
	}
	printf("===================== (ACPI TABLE) ======================\n");
	printf("RSDP: 0x%lx\n", (long)addr);
	printf("  signature: %s\n",  zstr(rsdp.signature, 8));
	printf("  checksum : %x\n",  rsdp.checksum);
	printf("  oem id   : %s\n",  zstr(rsdp.oem_id, 6));
	printf("  revision : %x\n",  rsdp.revision);
	printf("  rsdt addr: %lx\n", rsdp.rsdt_address);
	if (rsdp.revision == 2) {
		/* ACPI 2.0 uses XSDT */
		printf("  length   : %lx\n",  rsdp.length);
		printf("  xsdt addr: %llx\n", rsdp.xsdt_address);
		show_acpitab(rsdp.xsdt_address);
	}
	else {
		/* ACPI 1.x uses RSDT */
		show_acpitab(rsdp.rsdt_address);
	}
}

/*===============================================================
 *
 * Display MPBIOS Table
 *
 *===============================================================*/
const char *
mp_inttype(int id)
{
	switch(id) {
		case 0: return "int";
		case 1: return "nmi";
		case 2: return "smi";
		case 3: return "ext";
	}
	return "xxx";
}

void
show_mptable(uint32_t addr)
{
	mps_root      root;
	mps_header    hdr;
	mps_entry     entry;
	uint32_t      ptbl;
	uint8_t       type;
	int           idx;

	printf("===================== (MP TABLE) ======================\n");
	physmemcpy(&root, addr, sizeof(root));
	printf("Conf addr: %lx\n", root.mph_physaddr);
	printf("Length   : %x\n", root.mph_length);
	printf("Spec Rev : %x\n", root.mph_spec);
	printf("Checksum : %x\n", root.mph_cksum);
	printf("Feature  : %2x.%2x.%2x.%2x.%2x\n", 
			root.mph_feature[0], root.mph_feature[1], root.mph_feature[2],
			root.mph_feature[3], root.mph_feature[4]);

	ptbl = root.mph_physaddr + sizeof(hdr);
	physmemcpy(&hdr, root.mph_physaddr, sizeof(hdr));

	printf(" OEM ID   : '%s'\n", zstr(hdr.mpc_oem, 8));
	printf(" Prod ID  : '%s'\n", zstr(hdr.mpc_product, 12));
	printf(" Base len : %x\n", hdr.mpc_length);
	printf(" Checksum : %x\n", hdr.mpc_cksum);
	printf(" Ptr      : %x\n", hdr.mpc_oemptr);
	printf(" Size     : %x\n", hdr.mpc_oemsize);
	printf(" Count    : %x\n", hdr.mpc_oemcount);
	printf(" LAPIC    : %x\n", hdr.mpc_lapic);

	printf("---------\n");
	for(idx=0; idx<hdr.mpc_oemcount; idx++) {
		physmemcpy(&entry, ptbl, sizeof(entry));
		switch(entry.type) {
			case MPC_CPU:
				printf("  cpu : id:%.2x apicver:%.2x cpuflags:%.2x cpusig:%.08x fflags:%.08x\n",
						entry.m_cpu.apicid, entry.m_cpu.apicver, entry.m_cpu.cpuflag, 
						entry.m_cpu.cpufeature, entry.m_cpu.featureflag);
				ptbl += sizeof(mps_cpu);
				break;
			case MPC_BUS:
				printf("  bus : id:%.2x type:%s\n", 
						entry.m_bus.busid, zstr(entry.m_bus.bustype, 6));
				ptbl += sizeof(mps_bus);
				break;
			case MPC_APIC:
				printf("  apic: id:%.2x apicver:%.2x apicflag:%.2x addr:%.08x\n",
						entry.m_ioapic.apicid, entry.m_ioapic.apicver, entry.m_ioapic.flags, 
						entry.m_ioapic.apicaddr);
				ptbl += sizeof(mps_ioapic);
				break;
			case MPC_INT:
				printf("  int : type:%.2x[%s] pol:%x trig:%x bus:%.2x irq:%.2x -> apic:%.2x irq:%.2x\n",
						entry.m_int.irqtype, mp_inttype(entry.m_int.irqtype), entry.m_int.irqflag&3,
						(entry.m_int.irqflag>>2)&3, entry.m_int.srcbus, entry.m_int.srcbusirq,
						entry.m_int.dstapic, entry.m_int.dstirq);
				ptbl += sizeof(mps_intsrc);
				break;
			case MPC_LINT:
				printf("  lint: type:%.2x[%s] pol:%x trig:%x bus:%.2x irq:%.2x -> apic:%.2x lint:%.2x\n",
						entry.m_lint.irqtype, mp_inttype(entry.m_lint.irqtype), entry.m_lint.irqflag&3,
						(entry.m_lint.irqflag>>2)&3, entry.m_lint.srcbus, entry.m_lint.srcbusirq,
						entry.m_lint.dstapic, entry.m_lint.dstapiclint);
				ptbl += sizeof(mps_lintsrc);
				break;
			default:
				printf(" unknown: %x\n", type);
				return;
		}
	}
}

/* Uses neat feature of physmemcpy */
void
showtable(const char *name)
{
	FILE *fp;
	long  len;
	void *buf;

	set_physmemfile(name, 0);
	show_acpitab(0);
}

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-p filename] [-f filename]\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	uint32_t addr;
	int ch;

	while ((ch = getopt(argc, argv, "f:p:")) != -1) {
		switch (ch) {
		case 'f': /* save */
			savename = optarg;
			break;
		case 'p': /* print */
			showtable(optarg);
			return (0);
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}

	/* Dump MP Table */
	addr = scanmem(0xF0000L, 0xFFFFFL, 16, 4, "_MP_");
	if (addr != 0)
		show_mptable(addr);

	/* Dump ACPI Table */
	addr = scanmem(0xF0000L, 0xFFFFFL, 16, 8, "RSD PTR ");
	if (addr != 0)
		show_acpi(addr);

	return (0);
}
