#include "dlfcn.h"

#define INCL_BASE
#include <os2.h>

static ULONG retcode;
static char fail[300];

void *
dlopen(char *path, int mode)
{
	HMODULE handle;
	char tmp[260], *beg, *dot;
	ULONG rc;

	fail[0] = 0;
	if ((rc = DosLoadModule(fail, sizeof fail, path, &handle)) == 0)
		return (void *)handle;

	retcode = rc;

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
dlsym(void *handle, char *symbol)
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

	if (retcode == 0)
		return NULL;
	if (DosGetMessage(NULL, 0, buf, sizeof buf - 1, retcode,
			  "OSO001.MSG", &len)) {
		if (fail[0])
		  sprintf(buf, 
"OS/2 system error code %d, possible problematic module: '%s'",
			  retcode, fail);
		else
		  sprintf(buf, "OS/2 system error code %d", retcode);
	} else {
		buf[len] = '\0';
		if (len && buf[len - 1] == '\n')
			buf[--len] = 0;
		if (len && buf[len - 1] == '\r')
			buf[--len] = 0;
		if (len && buf[len - 1] == '.')
			buf[--len] = 0;
		if (fail[0] && len < 300)
		  sprintf(buf + len, ", possible problematic module: '%s'",
			  fail);
	}
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
