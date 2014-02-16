#include <sys/types.h>
#include <sys/exec_elf.h>

#include <assert.h>
#include <dlfcn.h>
#include <link.h>
#include <stddef.h>
#include <string.h>

static int
nonzero(char *s, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		if (s[i] != 0)
			return (1);

	return (0);
}

static int foundldso = 0;

static int
callback(struct dl_phdr_info *info, size_t size, void *cookie)
{
	int i;
	int foundrandomize = 0;

	assert(size >= sizeof(struct dl_phdr_info));

	if (strcmp(info->dlpi_name, "/usr/libexec/ld.so") != 0)
		return (0);
	foundldso = 1;

	for (i = 0; i < info->dlpi_phnum; i++)
		if (info->dlpi_phdr[i].p_type == PT_OPENBSD_RANDOMIZE) {
			foundrandomize = 1;
			assert(nonzero((char *)(info->dlpi_phdr[i].p_vaddr +
			    info->dlpi_addr), info->dlpi_phdr[i].p_memsz));
		}

	assert(foundrandomize);
	return (0);
}

int
main()
{
	dl_iterate_phdr(callback, NULL);
	assert(foundldso);
	return (0);
}
