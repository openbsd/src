/* $OpenBSD: acpiiort.h,v 1.1 2021/02/28 21:31:10 patrick Exp $ */
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

struct acpiiort_attach_args {
	struct acpi_iort_node	*aia_node;
	bus_space_tag_t		 aia_iot;
	bus_space_tag_t		 aia_memt;
	bus_dma_tag_t		 aia_dmat;
};

struct acpiiort_smmu {
	SIMPLEQ_ENTRY(acpiiort_smmu) as_list;
	struct acpi_iort_node	*as_node;
	void			*as_cookie;
	void			(*as_hook)(void *, uint32_t,
				    struct pci_attach_args *);
};

void acpiiort_smmu_register(struct acpiiort_smmu *);
void acpiiort_smmu_hook(struct acpi_iort_node *, uint32_t,
    struct pci_attach_args *);
