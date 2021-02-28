/* $OpenBSD: acpiiort.c,v 1.1 2021/02/28 21:31:10 patrick Exp $ */
/*
 * Copyright (c) 2021 Patrick Wildt <patrick@blueri.se>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

#include <arm64/dev/acpiiort.h>

SIMPLEQ_HEAD(, acpiiort_smmu) acpiiort_smmu_list =
    SIMPLEQ_HEAD_INITIALIZER(acpiiort_smmu_list);

int acpiiort_match(struct device *, void *, void *);
void acpiiort_attach(struct device *, struct device *, void *);

struct cfattach acpiiort_ca = {
	sizeof(struct device), acpiiort_match, acpiiort_attach
};

struct cfdriver acpiiort_cd = {
	NULL, "acpiiort", DV_DULL
};

int
acpiiort_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args *aaa = aux;
	struct acpi_table_header *hdr;

	/* If we do not have a table, it is not us */
	if (aaa->aaa_table == NULL)
		return 0;

	/* If it is an IORT table, we can attach */
	hdr = (struct acpi_table_header *)aaa->aaa_table;
	if (memcmp(hdr->signature, IORT_SIG, sizeof(IORT_SIG) - 1) != 0)
		return 0;

	return 1;
}

void
acpiiort_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpi_attach_args *aaa = aux;
	struct acpi_iort *iort = (struct acpi_iort *)aaa->aaa_table;
	struct acpi_iort_node *node;
	struct acpiiort_attach_args aia;
	uint32_t offset;
	int i;

	printf("\n");

	memset(&aia, 0, sizeof(aia));
	aia.aia_iot = aaa->aaa_iot;
	aia.aia_memt = aaa->aaa_memt;
	aia.aia_dmat = aaa->aaa_dmat;

	offset = iort->offset;
	for (i = 0; i < iort->number_of_nodes; i++) {
		node = (struct acpi_iort_node *)((char *)iort + offset);
		aia.aia_node = node;
		config_found(self, &aia, NULL);
		offset += node->length;
	}
}

void
acpiiort_smmu_register(struct acpiiort_smmu *as)
{
	SIMPLEQ_INSERT_TAIL(&acpiiort_smmu_list, as, as_list);
}

void
acpiiort_smmu_hook(struct acpi_iort_node *node, uint32_t rid,
    struct pci_attach_args *pa)
{
	struct acpiiort_smmu *as;

	SIMPLEQ_FOREACH(as, &acpiiort_smmu_list, as_list) {
		if (as->as_node == node) {
			as->as_hook(as->as_cookie, rid, pa);
			break;
		}
	}
}
