/* $Id: sectok.c,v 1.12 2003/04/04 00:50:56 deraadt Exp $ */

/*
copyright 2000
the regents of the university of michigan
all rights reserved

permission is granted to use, copy, create derivative works
and redistribute this software and such derivative works
for any purpose, so long as the name of the university of
michigan is not used in any advertising or publicity
pertaining to the use or distribution of this software
without specific, written prior authorization.  if the
above copyright notice or any other identification of the
university of michigan is included in any copy of any
portion of this software, then the disclaimer below must
also be included.

this software is provided as is, without representation
from the university of michigan as to its fitness for any
purpose, and without warranty by the university of
michigan of any kind, either express or implied, including
without limitation the implied warranties of
merchantability and fitness for a particular purpose. the
regents of the university of michigan shall not be liable
for any damages, including special, indirect, incidental, or
consequential damages, with respect to any claim arising
out of or in connection with the use of the software, even
if it has been or is hereafter advised of the possibility of
such damages.
*/

/*
 * common card routines
 *
 * Jim Rees
 * University of Michigan CITI, July 2001
 */

#ifdef __palmos__
#include <Common.h>
#include <System/SysAll.h>
#include <System/Unix/sys_types.h>
#include <System/Unix/unix_stdlib.h>
#include <System/Unix/unix_string.h>
#include <UI/UIAll.h>
#include "field.h"
#undef open
#else
#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <errno.h>
#endif

#include "sectok.h"
#include "ifdhandler.h"

#define MAX_READERS 32
#define N_DEFAULT_READERS 4

#define myisprint(x) ((x) >= '!' && (x) <= 'z')

#ifdef DL_READERS
static char defaultConfigFilePath[] = "/etc/reader.conf";
static char defaultDriverPath[] = "/usr/local/pcsc/lib/libtodos_ag.so";

int DBUpdateReaders(char *readerconf, int (callback) (int rn, unsigned long channelId, char *driverFile));

/* the callback for DBUpdateReaders */
static int addReader(int rn, unsigned long channelID, char *driverFile);
static void *lookupSym(void *handle, char *name);
#else
static int sillyports[] = {0x3f8, 0x2f8, 0x3e8, 0x2e8};
#endif

static int openReader(int readerNum, int flags);

typedef struct {
    unsigned long 	channelID;
    char 		*driverPath;
    unsigned int	driverLoaded;
    /* for now, i'm only worry about the "bare essentials" */
    u_long		(*open)(unsigned long channelID);
    u_long		(*close)(void);
    u_long		(*data)(struct SCARD_IO_HEADER junk,
				unsigned char *cmdData, unsigned long cmdLen,
				unsigned char *respData, unsigned long *respLen,
				struct SCARD_IO_HEADER *moreJunk);
    u_long		(*power)(unsigned long command);
    u_long		(*getcapa)(unsigned long selector, unsigned char *buffer);
    u_long		(*setcapa)(unsigned long selector, unsigned char *buffer);
    u_long		(*cardpresent)(void);
} readerInfo;

unsigned int numReaders;
readerInfo readers[MAX_READERS];

unsigned char root_fid[] = {0x3f, 0x00};

/*
  if (config_path != NULL and driver_name != NULL) error;
  if (config_path != NULL) use reader.conf there;
  if (driver_path != NULL) use the specified driver;
  if (config_path == NULL and driver_path == NULL) use /etc/reader.conf;

  Note that the config file is read only once, and drivers are only loaded once,
  so config_path and driver_path are ignored on subsequent calls.
*/

int
sectok_xopen(int rn, int flags, char *config_path, char *driver_path, int *swp)
{
    int r = 0;

#ifdef SCPERF
    SetTime ("scopen() start");
#endif /* SCPERF */

    if (rn < 0 || rn >= MAX_READERS) {
	r = STENOTTY;
	goto out;
    }

#ifdef DL_READERS
    if (driver_path) {
	/* caller specified a particular driver path to use */
	if (config_path) {
	    /* but also specified a config file, which is an error. */
	    r = STECNFFILES;
	    goto out;
	}
	if (!readers[rn].driverPath) {
	    /* need a driver */
	    if (addReader(rn, (0x10000 + rn), driver_path) < 0) {
		r = STEDRVR;
		goto out;
	    }
	}
    }

    if (numReaders == 0) {
	/* no drivers; read the config file */
	if (!config_path)
	    config_path = defaultConfigFilePath;
	if (DBUpdateReaders(config_path, addReader) < 0) {
	    int i;

	    if (config_path != defaultConfigFilePath) {
		/* Something wrong with caller's config file path. */
		r = STEDRVR;
		goto out;
	    }
	    /* This usually means there is no reader.conf.  Supply defaults. */
	    for (i = 0; i < N_DEFAULT_READERS; i++)
		addReader(i, (0x10000 | i), defaultDriverPath);
	}
    }
#else
    numReaders = N_DEFAULT_READERS;
#endif

    r = openReader(rn, flags);

#ifdef __palmos__
    if (sectok_swOK(r) && !sectok_cardpresent(rn))
	r = STENOCARD;
#else
    if (sectok_swOK(r) && !(flags & STONOWAIT)) {
	/* Wait for card present */
	while (!sectok_cardpresent(rn)) {
	    errno = 0;
	    sleep(1);
	    if (errno == EINTR) {
		r = STENOCARD;
		break;
	    }
	}
    }
#endif

 out:
#ifdef SCPERF
    SetTime ("scopen() end");
#endif /* SCPERF */

    if (swp)
	*swp = r;
    return (!sectok_swOK(r) ? -1 : rn);
}

int sectok_open(int rn, int flags, int *swp)
{
    return sectok_xopen(rn, flags, NULL, NULL, swp);
}

int sectok_friendly_open(const char *rn, int flags, int *swp)
{
    /* just convert the reader to a integer for now */
    if (rn != NULL) {
        return sectok_xopen(atoi(rn), flags, NULL, NULL, swp);
    } else {
        return -1;
    }
}

static int
openReader(int readerNum, int flags)
{
    readerInfo *reader;

#ifdef DEBUG
    printf("openReader %d\n", readerNum);
#endif

    if (readerNum < 0 || readerNum >= MAX_READERS)
	return STEDRVR;
    reader = &readers[readerNum];

    if (!reader->driverLoaded) {
#ifdef DL_READERS
	void *libHandle;

	if (!reader->driverPath)
	    return STEDRVR;
	libHandle = dlopen(reader->driverPath, RTLD_LAZY);
	if (!libHandle) {
#ifdef DEBUG
	    printf("%s: %s\n", reader->driverPath, dlerror());
#endif
	    return STEDRVR;
	}
	reader->open = lookupSym(libHandle, "IO_Create_Channel");
	if (reader->open == NULL)
	    return STEDRVR;

	reader->close = lookupSym(libHandle, "IO_Close_Channel");
	if (reader->close == NULL)
	    return STEDRVR;

	reader->data = lookupSym(libHandle, "IFD_Transmit_to_ICC");
	if (reader->data == NULL)
	    return STEDRVR;

	reader->power = lookupSym(libHandle, "IFD_Power_ICC");
	if (reader->power == NULL)
	    return STEDRVR;

	reader->getcapa = lookupSym(libHandle, "IFD_Get_Capabilities");
	if (reader->getcapa == NULL)
	    return STEDRVR;

	reader->setcapa = lookupSym(libHandle, "IFD_Set_Capabilities");
	if (reader->setcapa == NULL)
	    return STEDRVR;

	reader->cardpresent = lookupSym(libHandle, "IFD_Is_ICC_Present");
#else /* DL_READERS */
	reader->open = IO_Create_Channel;
	reader->close = IO_Close_Channel;
	reader->data = IFD_Transmit_to_ICC;
	reader->power = IFD_Power_ICC;
	reader->getcapa = IFD_Get_Capabilities;
	reader->setcapa = IFD_Set_Capabilities;
	reader->cardpresent = IFD_Is_ICC_Present;
	reader->channelID = (0x10000 | (readerNum < 4 ? sillyports[readerNum] : readerNum));
#endif /* DL_READERS */

	reader->driverLoaded = 1;
    }

    /* send flags to the driver */
    flags ^= STONOWAIT;
    reader->setcapa(SCTAG_OPEN_FLAGS, (u_char *)&flags);
    /* if this returns an error, setcapa is not supported in this driver,
       but that's OK. */

    if (reader->open(reader->channelID))
	return STECOMM;
    else
	return STEOK;
}

int
sectok_close(int fd)
{
    readerInfo *reader = &readers[fd];

    if (fd < 0 || fd >= MAX_READERS)
	return -1;

    reader = &readers[fd];

    if (!reader->driverLoaded)
	return -1;

    return (reader->close()) ? -1 : 0;
}

int
sectok_cardpresent(int fd)
{
    readerInfo *reader = &readers[fd];
    unsigned long v;

    if (!reader->driverLoaded)
	return 0;

    if (reader->cardpresent)
	v = reader->cardpresent();
    else if (reader->getcapa(SCTAG_IFD_CARDPRESENT, (unsigned char *) &v))
	return 1;

    return (v == IFD_ICC_PRESENT || v == 0) ? 1 : 0;
}

int
sectok_reset(int fd, int flags, unsigned char *atr, int *swp)
{
    readerInfo *reader = &readers[fd];
    int n = 0, r = STEOK;

#ifdef SCPERF
    SetTime ("scxreset() start");
#endif /* SCPERF */

    if (!reader->driverLoaded) {
	r = STECLOSED;
	goto out;
    }

    if (!sectok_cardpresent(fd)) {
	r = STENOCARD;
	goto out;
    }

    /* send flags to the driver */
    reader->setcapa(SCTAG_RESET_FLAGS, (u_char *)&flags);
    /* if this returns an error, setcapa is not supported in this driver,
       but that's OK. */

    if (reader->power(IFD_RESET)) {
#ifdef DEBUG
	printf("power failed!\n");
#endif
	r = STESLAG;
	goto out;
    }

    if (atr && reader->getcapa(TAG_IFD_ATR, atr)) {
#ifdef DEBUG
	printf("reset failed!\n");
#endif
	r = STESLAG;
	goto out;
    }

    if (reader->getcapa(SCTAG_IFD_ATRLEN, (unsigned char *) &n) || n <= 0) {
	/* can't get atr len, take a wild guess */
	if (atr) {
	    for (n = MAX_ATR_SIZE - 1; !atr[n]; n--)
		;
	    n--;
	} else
	    n = MAX_ATR_SIZE;
    }

 out:
    if (swp)
	*swp = r;

#ifdef SCPERF
    SetTime ("scxreset() end");
#endif /* SCPERF */

    return n;
}

#ifdef DL_READERS
static int
addReader(int rn, unsigned long channelID, char *driverFile)
{
    readerInfo *reader;

    if (rn < 0 || rn >= MAX_READERS)
	return -1;

    reader = &readers[rn];

    if (reader->driverPath)
	return -1;

    reader->channelID = channelID;
    reader->driverPath = strdup(driverFile);
    reader->driverLoaded = 0;
    numReaders++;
    return 0;
}

static void *
lookupSym(void *handle, char *name)
{
#ifdef __linux__
    return dlsym(handle, name);
#elif __sun
    return dlsym(handle, name);
#else
    char undername[32];

    snprintf(undername, sizeof undername, "_%s", name);
    return dlsym(handle, undername);
#endif
}
#endif /* DL_READERS */

int
sectok_apdu(int fd, int cla, int ins, int p1, int p2,
	    int ilen, unsigned char *ibuf, int olen, unsigned char *obuf, int *swp)
{
    unsigned char cmd[6+255], rsp[255+2];
    unsigned long n;
    int le;
    readerInfo *reader = &readers[fd];
    struct SCARD_IO_HEADER garbage;

    if (reader->driverLoaded == 0) {
	*swp = STECLOSED;
	return -1;
    }

    cmd[0] = cla;
    cmd[1] = ins;
    cmd[2] = p1;
    cmd[3] = p2;

    ilen &= 0xff;
    le = (255 < olen) ? 255 : olen;

    if (ilen && ibuf) {
	/* Send "in" data */
	cmd[4] = ilen;
	memcpy(&cmd[5], ibuf, ilen);
	ilen += 5;
	if (le)
	    cmd[ilen++] = le;
	n = obuf ? sizeof rsp : 2;
	if (reader->data(garbage, cmd, ilen, rsp, &n, NULL) || n < 2) {
	    *swp = STECOMM;
	    return -1;
	}
	if (rsp[n-2] == 0x61 && olen && obuf) {
	    /* Response available; get it (driver should do this but some don't) */
	    cmd[1] = 0xc0;
	    cmd[2] = cmd[3] = 0;
	    cmd[4] = rsp[n-1];
	    n = sizeof rsp;
	    if (reader->data(garbage, cmd, 5, rsp, &n, NULL)) {
		*swp = STECOMM;
		return -1;
	    }
	}
    } else {
	/* Get "out" data */
	cmd[4] = olen;
	n = sizeof rsp;
	if (reader->data(garbage, cmd, 5, rsp, &n, NULL)) {
	    *swp = STECOMM;
	    return -1;
	}
    }

    if (n >= 2) {
	*swp = sectok_mksw(rsp[n-2], rsp[n-1]);
	n -= 2;
    } else {
	/* This shouldn't happen; apdu ok but no status available */
	*swp = STEOK;
    }

    if (n && olen)
	memcpy(obuf, rsp, (n < olen) ? n : olen);

    return n;
}

void
sectok_fmt_fid(char *fname, size_t fnamelen, unsigned char *fid)
{
    int f0 = fid[0], f1 = fid[1];

    if (f0 == 0x3f && f1 == 0)
	snprintf(fname, fnamelen, "/");
    else if (myisprint(f0) && f1 == 0)
	snprintf(fname, fnamelen, "%c", f0);
    else if (myisprint(f0) && myisprint(f1))
	snprintf(fname, fnamelen, "%c%c", f0, f1);
    else
	snprintf(fname, fnamelen, "%02x%02x", f0, f1);
}

int
sectok_selectfile(int fd, int cla, unsigned char *fid, int *swp)
{
    unsigned char obuf[256];

    sectok_apdu(fd, cla, 0xa4, 0, 0, 2, fid, sizeof obuf, obuf, swp);
    if (!sectok_swOK(*swp))
	return -1;

    return 0;
}
