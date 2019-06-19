/* Public domain. */

#ifndef _LINUX_ACPI_H
#define _LINUX_ACPI_H

#include <sys/types.h>
#include <sys/param.h>

#ifdef __HAVE_ACPI
#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#endif

typedef size_t acpi_size;
typedef int acpi_status;

struct acpi_table_header;

#define ACPI_SUCCESS(x) ((x) == 0)

#define AE_NOT_FOUND	0x0005

acpi_status acpi_get_table(const char *, int, struct acpi_table_header **);

#endif
