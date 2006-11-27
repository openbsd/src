/* $OpenBSD: dsdt.h,v 1.20 2006/11/27 15:17:37 jordan Exp $ */
/*
 * Copyright (c) 2005 Marco Peereboom <marco@openbsd.org>
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

#ifndef __DEV_ACPI_DSDT_H__
#define __DEV_ACPI_DSDT_H__

struct aml_vallist
{
	struct aml_value *obj;
	int nobj;
	struct aml_vallist *next;
};

struct aml_scope
{
	struct acpi_softc  *sc;
	uint8_t            *pos;
	uint8_t            *end;
	struct aml_node    *node;
	struct aml_vallist *tmpvals;
	struct aml_scope   *parent;
	struct aml_value   *locals;
	struct aml_value   *args;
	int                 nargs;
};


struct aml_opcode
{
	u_int32_t		opcode;
	const char		*mnem;
	const char		*args;
	struct aml_value      *(*handler)(struct aml_scope *,int,struct aml_value*);
};

const char		*aml_eisaid(u_int32_t);
const char              *aml_args(int);
const char		*aml_mnem(int);
int64_t			aml_val2int(struct aml_value *);
struct aml_node		*aml_searchname(struct aml_node *, const void *);
struct aml_node         *aml_createname(struct aml_node *, const void *, struct aml_value *);

struct aml_value	*aml_allocint(uint64_t);
struct aml_value	*aml_allocstr(const char *);
struct aml_value	*aml_allocvalue(int, int64_t, const void *);
void			aml_freevalue(struct aml_value *);
void			aml_notify(struct aml_node *, int);
void			aml_notify_dev(const char *, int);
void			aml_showvalue(struct aml_value *, int);
void			aml_walkroot(void);
void			aml_walktree(struct aml_node *);

int			aml_find_node(struct aml_node *, const char *,
			    void (*)(struct aml_node *, void *), void *);
int			acpi_parse_aml(struct acpi_softc *, u_int8_t *,
			    u_int32_t);
int			aml_eval_object(struct acpi_softc *, struct aml_node *,
			    struct aml_value *, int, struct aml_value *);
void			aml_register_notify(struct aml_node *, const char *,
			    int (*)(struct aml_node *, int, void *), void *);

u_int64_t aml_getpciaddr(struct acpi_softc *, struct aml_node *);

int aml_evalnode(struct acpi_softc *, struct aml_node *,
		 int , struct aml_value *,
		 struct aml_value *);

int aml_evalname(struct acpi_softc *, struct aml_node *, 
		 const char *, int, struct aml_value *,
		 struct aml_value *);

void aml_fixup_dsdt(u_int8_t *, u_int8_t *, int);
void aml_create_defaultobjects(void);

int acpi_mutex_acquire(struct aml_value *, int);
void acpi_mutex_release(struct aml_value *);

const char *aml_nodename(struct aml_node *);

#define SR_IRQ                  0x04
#define SR_DMA                  0x05
#define SR_STARTDEP             0x06
#define SR_ENDDEP               0x07
#define SR_IOPORT               0x08
#define SR_FIXEDPORT            0x09
#define SR_ENDTAG               0x0F

#define LR_24BIT                0x81
#define LR_GENREGISTER          0x82
#define LR_32BIT                0x85
#define LR_32BITFIXED           0x86
#define LR_DWORD                0x87
#define LR_WORD                 0x88
#define LR_EXTIRQ               0x89
#define LR_QWORD                0x8A

union acpi_resource
{
	struct {
		uint8_t  typecode;
		uint16_t length;
	}  __packed hdr;
	
        /* Small resource structures
	 * format of typecode is: tttttlll, t = type, l = length 
	 */
        struct {
                uint8_t  typecode;
                uint16_t irq_mask;
                uint8_t  irq_info;
        }  __packed sr_irq;
        struct {
                uint8_t  typecode;
                uint8_t  dma_chan;
                uint8_t  dma_info;
        }  __packed sr_dma;
        struct {
                uint8_t  typecode;
                uint8_t  io_info;
                uint16_t io_min;
                uint16_t io_max;
                uint8_t  io_aln;
                uint8_t  io_len;
        }  __packed sr_ioport;
        struct {
                uint8_t  typecode;
                uint16_t fio_bas;
                uint8_t  fio_len;
        }  __packed sr_fioport;

        /* Large resource structures */
        struct {
                uint8_t  typecode;
                uint16_t length;
                uint8_t  m24_info;
                uint16_t m24_min;
                uint16_t m24_max;
                uint16_t m24_aln;
                uint16_t m24_len;
        }  __packed lr_m24;
        struct {
                uint8_t  typecode;
                uint16_t length;
                uint8_t  m32_info;
                uint32_t m32_min;
                uint32_t m32_max;
                uint32_t m32_aln;
                uint32_t m32_len;
        }  __packed lr_m32;
	struct {
		uint8_t  typecode;
		uint16_t length;
		uint8_t  flags;
		uint8_t  irq_count;
		uint32_t irq[1];
	} __packed lr_extirq;
} __packed;

#define AML_CRSTYPE(x) ((x)->hdr.typecode & 0x80 ? \
			(x)->hdr.typecode : \
(x)->hdr.typecode >> 3)
#define AML_CRSLEN(x) ((x)->hdr.typecode & 0x80 ? \
		       (x)->hdr.length+2 : \
(x)->hdr.length & 0x7)

int aml_print_resource(union acpi_resource *, void *);
int aml_parse_resource(int, uint8_t *, int (*)(union acpi_resource *, void *),
    void *);

#define ACPI_E_NOERROR   0x00
#define ACPI_E_BADVALUE  0x01

#define AML_MAX_ARG	 7
#define AML_MAX_LOCAL	 8

/* XXX: endian macros */
#define aml_letohost16(x) letoh16(x)
#define aml_letohost32(x) letoh32(x)
#define aml_letohost64(x) letoh64(x)

#define AML_WALK_PRE 0x00
#define AML_WALK_POST 0x01
void
aml_walknodes(struct aml_node *, int,
	      int (*)(struct aml_node *, void *),
	      void *);

void aml_postparse(void);

#endif /* __DEV_ACPI_DSDT_H__ */
