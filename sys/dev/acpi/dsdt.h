/* $OpenBSD: dsdt.h,v 1.5 2006/02/03 23:55:47 jordan Exp $ */
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

const char	*aml_eisaid(u_int32_t);
int		aml_find_node(struct aml_node *, const char *,
		    void (*)(struct aml_node *, void *), void *);
int	acpi_parse_aml(struct acpi_softc *, u_int8_t *, u_int32_t);

int     aml_eval_object(struct acpi_softc *, struct aml_node *, 
			struct aml_value *, int, struct aml_value *);
int     aml_eval_name(struct acpi_softc *, struct aml_node *, const char *,
		      struct aml_value *, struct aml_value *);
void    aml_showvalue(struct aml_value *);

void    aml_walktree(struct aml_node *);

struct aml_value *aml_allocint(uint64_t, int);
struct aml_value *aml_allocstr(const char *);
struct aml_value *aml_allocvalue(int, int64_t, void *, const char *);
struct aml_value *aml_copyvalue(const struct aml_value *);

int aml_freevalue(struct aml_value **);
int aml_comparevalue(int, const struct aml_value *, const struct aml_value *);

#endif /* __DEV_ACPI_DSDT_H__ */
