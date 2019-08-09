/*	$OpenBSD: smi.h,v 1.1 2019/08/09 06:17:59 martijn Exp $	*/

/*
 * Copyright (c) 2019 Martijn van Duren <martijn@openbsd.org>
 * Copyright (c) 2007, 2008 Reyk Floeter <reyk@openbsd.org>
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

#include <sys/tree.h>
#include <sys/queue.h>

#define OID_ROOT		0x00
#define OID_RD			0x01
#define OID_WR			0x02
#define OID_IFSET		0x04	/* only if user-specified value */
#define OID_DYNAMIC		0x08	/* free allocated data */
#define OID_TABLE		0x10	/* dynamic sub-elements */
#define OID_MIB			0x20	/* root-OID of a supported MIB */
#define OID_KEY			0x40	/* lookup tables */
#define	OID_REGISTERED		0x80	/* OID registered by subagent */

#define OID_RS			(OID_RD|OID_IFSET)
#define OID_WS			(OID_WR|OID_IFSET)
#define OID_RW			(OID_RD|OID_WR)
#define OID_RWS			(OID_RW|OID_IFSET)

#define OID_TRD			(OID_RD|OID_TABLE)
#define OID_TWR			(OID_WR|OID_TABLE)
#define OID_TRS			(OID_RD|OID_IFSET|OID_TABLE)
#define OID_TWS			(OID_WR|OID_IFSET|OID_TABLE)
#define OID_TRW			(OID_RD|OID_WR|OID_TABLE)
#define OID_TRWS		(OID_RW|OID_IFSET|OID_TABLE)

enum smi_output_string {
	smi_os_default,
	smi_os_hex,
	smi_os_ascii
};

enum smi_oid_lookup {
	smi_oidl_numeric,
	smi_oidl_short,
	smi_oidl_full
};

struct oid {
	struct ber_oid		 o_id;
#define o_oid			 o_id.bo_id
#define o_oidlen		 o_id.bo_n

	char			*o_name;

	u_int			 o_flags;

	int			 (*o_get)(struct oid *, struct ber_oid *,
				    struct ber_element **);
	int			 (*o_set)(struct oid *, struct ber_oid *,
				    struct ber_element **);
	struct ber_oid		*(*o_table)(struct oid *, struct ber_oid *,
				    struct ber_oid *);

	long long		 o_val;
	void			*o_data;

	struct ctl_conn		*o_session;

	RB_ENTRY(oid)		 o_element;
	RB_ENTRY(oid)		 o_keyword;
	TAILQ_ENTRY(oid)	 o_list;
};

int smi_init(void);
unsigned int smi_application(struct ber_element *);
int smi_string2oid(const char *, struct ber_oid *);
char *smi_oid2string(struct ber_oid *, char *, size_t, enum smi_oid_lookup);
void smi_mibtree(struct oid *);
struct oid *smi_foreach(struct oid *, u_int);
void smi_debug_elements(struct ber_element *);
char *smi_print_element(struct ber_element *, int, enum smi_output_string,
    enum smi_oid_lookup);
