/* $Id: sc7816.c,v 1.5 2001/06/26 16:26:14 deraadt Exp $ */

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
 * sc7816 library for use with pc/sc ifd drivers
 *
 * Jim Rees
 * Mukesh Agrawal
 * University of Michigan CITI, August 2000
 */

#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

#ifdef SCPERF
#define SCPERF_FIRST_APPEARANCE
#endif /* SCPERF */

#include "sectok.h"
#include "ifdhandler.h"

#define MAX_READERS 32
#define N_DEFAULT_READERS 4

#ifdef DL_READERS
static char defaultConfigFilePath[] = "/etc/reader.conf";
static char defaultDriverPath[] = "/usr/local/pcsc/lib/libtodos_ag.so";

int DBUpdateReaders(char *readerconf, int (callback) (int rn, unsigned long channelId, char *driverFile));

/* the callback for DBUpdateReaders */
int addReader(int rn, unsigned long channelID, char *driverFile);
void *lookupSym(void *handle, char *name);
#endif

int openReader(int readerNum, int flags);

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

char *scerrtab[] = {
    "ok",
    "no such tty",
    "out of memory",
    "timeout",
    "slag!",
    "card type not supported",
    "no card in reader",
    "not implemented",
    "error loading driver",
    "communications error",
    "reader not open",
    "unknown error",
};

unsigned int numReaders;
readerInfo readers[MAX_READERS];

/* NI, 11/1/2000 : Now the body of scopen() is in scxopen() to allow
   specifing a path to the config file or the driver.  scopen() is
   an entry function for scxopen() to maintain backward compatibility. */
int
scopen(int ttyn, int flags, int *ep)
{
    return scxopen (ttyn, flags, ep, NULL, NULL);
}

/*
  if (config_path != NULL and driver_name != NULL) error;
  if (config_path != NULL) use reader.conf there;
  if (driver_path != NULL) use the specified driver;
  if (config_path == NULL and driver_path == NULL) use /etc/reader.conf;

  Note that the config file is read only once, and drivers are only loaded once,
  so config_path and driver_path are ignored on subsequent calls.
*/

int
scxopen(int ttyn, int flags, int *ep, char *config_path, char *driver_path)
{
    int r = 0;

#ifdef SCPERF
    SetTime ("scopen() start");
#endif /* SCPERF */

    if (ttyn < 0 || ttyn >= MAX_READERS) {
	r = SCENOTTY;
	goto out;
    }

#ifdef DL_READERS
    if (driver_path) {
	/* caller specified a particular driver path to use */
	if (config_path) {
	    /* but also specified a config file, which is an error. */
	    r = SCECNFFILES;
	    goto out;
	}
	if (!readers[ttyn].driverPath) {
	    /* need a driver */
	    if (addReader(ttyn, (0x10000 + ttyn), driver_path) < 0) {
		r = SCEDRVR;
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
		r = SCEDRVR;
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

    r = openReader(ttyn, flags);

    if (!r && (flags & SCODSR)) {
	/* Wait for card present */
	while (!sccardpresent(ttyn))
	    sleep(1);
    }

 out:
#ifdef SCPERF
    SetTime ("scopen() end");
#endif /* SCPERF */

    if (ep)
	*ep = r;
    return r ? -1 : ttyn;
}

int
openReader(int readerNum, int flags)
{
    readerInfo *reader;

#ifdef DEBUG
    fprintf(stderr, "openReader %d\n", readerNum);
#endif

    if (readerNum < 0 || readerNum >= MAX_READERS)
	return SCEDRVR;
    reader = &readers[readerNum];

    if (!reader->driverLoaded) {
#ifdef DL_READERS
	void *libHandle;

	if (!reader->driverPath)
	    return SCEDRVR;
	libHandle = dlopen(reader->driverPath, RTLD_LAZY);
	if (!libHandle) {
#ifdef DEBUG
	    fprintf(stderr, "%s: %s\n", reader->driverPath, dlerror());
#endif
	    return SCEDRVR;
	}
	reader->open = lookupSym(libHandle, "IO_Create_Channel");
	if (reader->open == NULL)
	    return SCEDRVR;

	reader->close = lookupSym(libHandle, "IO_Close_Channel");
	if (reader->close == NULL)
	    return SCEDRVR;

	reader->data = lookupSym(libHandle, "IFD_Transmit_to_ICC");
	if (reader->data == NULL)
	    return SCEDRVR;

	reader->power = lookupSym(libHandle, "IFD_Power_ICC");
	if (reader->power == NULL)
	    return SCEDRVR;

	reader->getcapa = lookupSym(libHandle, "IFD_Get_Capabilities");
	if (reader->getcapa == NULL)
	    return SCEDRVR;

	reader->setcapa = lookupSym(libHandle, "IFD_Set_Capabilities");
	if (reader->setcapa == NULL)
	    return SCEDRVR;

	reader->cardpresent = lookupSym(libHandle, "IFD_Is_ICC_Present");
#else /* DL_READERS */
	reader->open = IO_Create_Channel;
	reader->close = IO_Close_Channel;
	reader->data = IFD_Transmit_to_ICC;
	reader->power = IFD_Power_ICC;
	reader->getcapa = IFD_Get_Capabilities;
	reader->setcapa = IFD_Set_Capabilities;
	reader->cardpresent = IFD_Is_ICC_Present;
	reader->channelID = (0x10000 | readerNum);
#endif /* DL_READERS */

	reader->driverLoaded = 1;
    }

    /* send flags to the driver */
    reader->setcapa(SCTAG_OPEN_FLAGS, (u_char *)&flags);
    /* if this returns an error, setcapa is not supported in this driver,
       but that's OK. */

    if (reader->open(reader->channelID))
	return SCECOMM;
    else
	return 0;
}

int
scclose(int ttyn)
{
    readerInfo *reader = &readers[ttyn];

    if (ttyn < 0 || ttyn >= MAX_READERS)
	return -1;

    reader = &readers[ttyn];

    if (!reader->driverLoaded)
	return -1;

    return (reader->close()) ? -1 : 0;
}

int
sccardpresent(int ttyn)
{
    readerInfo *reader = &readers[ttyn];
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
scxreset(int ttyn, int flags, unsigned char *atr, int *ep)
{
    readerInfo *reader = &readers[ttyn];
    int n = 0, r = SCEOK;
    struct scparam param;

#ifdef SCPERF
    SetTime ("scxreset() start");
#endif /* SCPERF */

    if (!reader->driverLoaded) {
	r = SCECLOSED;
	goto out;
    }

    if (!sccardpresent(ttyn)) {
	r = SCENOCARD;
	goto out;
    }

    /* send flags to the driver */
    reader->setcapa(SCTAG_RESET_FLAGS, (u_char *)&flags);
    /* if this returns an error, setcapa is not supported in this driver,
       but that's OK. */

    if (reader->power(IFD_RESET)) {
#ifdef DEBUG
	fprintf(stderr, "power failed!\n");
#endif
	r = SCESLAG;
	goto out;
    }

    if (atr && reader->getcapa(TAG_IFD_ATR, atr)) {
#ifdef DEBUG
	fprintf(stderr, "reset failed!\n");
#endif
	r = SCESLAG;
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

    if (flags & SCRV)
	parse_atr(-1, flags, atr, n, &param);

 out:
    if (ep)
	*ep = r;

#ifdef SCPERF
    SetTime ("scxreset() end");
#endif /* SCPERF */

    return n;
}

int
screset(int ttyn, unsigned char *atr, int *ep)
{
    return scxreset(ttyn, 0, atr, ep);
}

int
scrw(int ttyn, int cla, int ins, int p1, int p2, int ilen, unsigned char *ibuf, int olen, unsigned char *obuf, int *sw1p, int *sw2p)
{
    unsigned char cmd[6+255], rsp[255+2];
    unsigned long n;
    int le;
    readerInfo *reader = &readers[ttyn];
    struct SCARD_IO_HEADER garbage;

    if (reader->driverLoaded == 0)
	return SCECLOSED;

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
	if (reader->data(garbage, cmd, ilen, rsp, &n, NULL) || n < 2)
	    return -1;
	if (rsp[n-2] == 0x61 && olen && obuf) {
	    /* Response available; get it (driver should do this but some don't) */
	    cmd[1] = 0xc0;
	    cmd[2] = cmd[3] = 0;
	    cmd[4] = rsp[n-1];
	    n = sizeof rsp;
	    if (reader->data(garbage, cmd, 5, rsp, &n, NULL))
		return -1;
	}
    } else {
	/* Get "out" data */
	cmd[4] = olen;
	n = sizeof rsp;
	if (reader->data(garbage, cmd, 5, rsp, &n, NULL))
	    return -1;
    }

    if (n >= 2) {
	*sw1p = rsp[n-2];
	*sw2p = rsp[n-1];
	n -= 2;
    }

    if (n && olen)
	memcpy(obuf, rsp, (n < olen) ? n : olen);

    return n;
}

int
scwrite(int ttyn, int cla, int ins, int p1, int p2, int p3, unsigned char *buf, int *sw1p, int *sw2p)
{
    int rv;
#ifdef SCPERF
    char *scperf_buf;

    scperf_buf = malloc (64);

    sprintf (scperf_buf, "scwrite (ins %02x, p3 %02x) start", ins, p3);
    SetTime(scperf_buf);
#endif /* SCPERF */
    rv = scrw(ttyn, cla, ins, p1, p2, p3, buf, 0, NULL, sw1p, sw2p);

#ifdef SCPERF
    SetTime("scwrite() end");
#endif /* SCPERF */
    return (rv >= 0) ? p3 : -1;
}

int
scread(int ttyn, int cla, int ins, int p1, int p2, int p3, unsigned char *buf, int *sw1p, int *sw2p)
{
    int rv;
#ifdef SCPERF
    char *scperf_buf;

    scperf_buf = malloc (64);

    sprintf (scperf_buf, "scread (ins %02x, p3 %02x) start", ins, p3);
    SetTime(scperf_buf);
#endif /* SCPERF */
    rv = scrw(ttyn, cla, ins, p1, p2, 0, NULL, p3, buf, sw1p, sw2p);

#ifdef SCPERF
    SetTime("scread() end");
#endif /* SCPERF */
    return rv;
}

#ifdef DL_READERS
int
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

void *
lookupSym(void *handle, char *name)
{
#ifdef __linux__
    return dlsym(handle, name);
#elif __sun
    return dlsym(handle, name);
#else
    char undername[32];

    sprintf(undername, "_%s", name);
    return dlsym(handle, undername);
#endif
}
#endif /* DL_READERS */
