#include "dlfcn.h"
#include "string.h"
#include "stdio.h"

#define INCL_BASE
#include <os2.h>

static ULONG retcode;
static char fail[300];

char *os2error(int rc);

void *
dlopen(const char *path, int mode)
{
	HMODULE handle;
	char tmp[260], *beg, *dot;
	ULONG rc;

	fail[0] = 0;
	if ((rc = DosLoadModule(fail, sizeof fail, (char*)path, &handle)) == 0)
		return (void *)handle;

	retcode = rc;

	if (strlen(path) >= sizeof(tmp))
	    return NULL;

	/* Not found. Check for non-FAT name and try truncated name. */
	/* Don't know if this helps though... */
	for (beg = dot = path + strlen(path);
	     beg > path && !strchr(":/\\", *(beg-1));
	     beg--)
		if (*beg == '.')
			dot = beg;
	if (dot - beg > 8) {
		int n = beg+8-path;

		memmove(tmp, path, n);
		memmove(tmp+n, dot, strlen(dot)+1);
		if (DosLoadModule(fail, sizeof fail, tmp, &handle) == 0)
			return (void *)handle;
	}

	return NULL;
}

void *
dlsym(void *handle, const char *symbol)
{
	ULONG rc, type;
	PFN addr;

	fail[0] = 0;
	rc = DosQueryProcAddr((HMODULE)handle, 0, symbol, &addr);
	if (rc == 0) {
		rc = DosQueryProcType((HMODULE)handle, 0, symbol, &type);
		if (rc == 0 && type == PT_32BIT)
			return (void *)addr;
		rc = ERROR_CALL_NOT_IMPLEMENTED;
	}
	retcode = rc;
	return NULL;
}

char *
dlerror(void)
{
	static char buf[700];
	ULONG len;
	char *err;

	if (retcode == 0)
		return NULL;
	err = os2error(retcode);
	len = strlen(err);
	if (len > sizeof(buf) - 1)
	    len = sizeof(buf) - 1;
	strncpy(buf, err, len+1);
	if (fail[0] && len < 300)
	    sprintf(buf + len, ", possible problematic module: '%s'", fail);
	retcode = 0;
	return buf;
}

int
dlclose(void *handle)
{
	ULONG rc;

	if ((rc = DosFreeModule((HMODULE)handle)) == 0) return 0;

	retcode = rc;
	return 2;
}
