/*
 * sc7816 library for use with pc/sc ifd drivers
 *
 * Jim Rees
 * Mukesh Agrawal
 * University of Michigan CITI, August 2000
 */
static char *rcsid = "$Id: sc7816.c,v 1.1 2001/05/22 15:35:58 rees Exp $";

#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

#ifdef SCPERF 
#define SCPERF_FIRST_APPEARANCE
#endif /* SCPERF */

#include "sectok.h"

#define MAX_READERS 32

/* pcsc cruft */
#define MAX_ATR_SIZE 33
#define IFD_ICC_PRESENT 615
#define IFD_RESET 502
#define TAG_IFD_ATR 0x303

struct SCARD_IO_HEADER {
    unsigned long Protocol, Length;
};

int DBUpdateReaders(char *readerconf, void (callback) (char *name, unsigned long channelId, char *driverFile));

/* the callback for DBUpdateReaders */
void addReader(char *name, unsigned long channelID, char *driverFile);

int openReader(int readerNum, int flags);
void *lookupSym(void *handle, char *name);

typedef struct {
    unsigned long 	channelID;
    char 		*driverPath;
    unsigned int	driverLoaded;
    /* for now, i'm only worry about the "bare essentials" */
    long		(*open)(unsigned long channelID);
    long		(*close)(void);
    long		(*data)(struct SCARD_IO_HEADER junk,
				unsigned char *cmdData, unsigned long cmdLen,
				unsigned char *respData, unsigned long *respLen,
				struct SCARD_IO_HEADER *moreJunk);
    long		(*power)(unsigned long command);
    long		(*getcapa)(unsigned long selector, unsigned char *buffer);
    long		(*setcapa)(unsigned long selector, unsigned char *buffer);
    long		(*cardpresent)(void);
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

static char defaultConfigFilePath[] = "/etc/sectok.conf";

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
  NI:
  
  if (config_path != NULL and driver_name != NULL) error; 
  if (config_path != NULL) use sectok.conf there; 
  if (driver_path != NULL) use the specified driver; 
  if (config_path == NULL and driver_path == NULL) use /etc/sectok.conf; 
*/
int
scxopen(int ttyn, int flags, int *ep,
	char *config_path, char *driver_path)
{
    int i, r;
    static char todos_driver_path[] = "/usr/local/pcsc/lib/libtodos_ag.so";
    char *configFilePath = defaultConfigFilePath; 
    
#ifdef SCPERF
    SetTime ("scopen() start");
#endif /* SCPERF */

    if (config_path != NULL && driver_path != NULL) {
	/* both config path and driver path are
	   specified.  thus conflict. */
	return SCECNFFILES;
    }
    else if (config_path != NULL) {
	/* config file specified */
	configFilePath = config_path; 
    }
    else if (driver_path != NULL) {
	/* driver path is specified */
	numReaders = 0;
	
	addReader(NULL, 0x10000, driver_path);
	addReader(NULL, 0x10001, driver_path);

	goto open_readers; 
    }

    for (i = 0; i < numReaders; i++) {
	if (readers[i].driverPath) {
	    free(readers[i].driverPath);
	    readers[i].driverPath = NULL;
	}
    }
    numReaders = 0;

    if (DBUpdateReaders(configFilePath, addReader) < 0) {
	/* This usually means there is no sectok.conf.  Supply a default. */
	addReader(NULL, 0x10000, todos_driver_path);
	addReader(NULL, 0x10001, todos_driver_path);
    }

 open_readers:
    r = openReader(ttyn, flags);
    if (ep)
	*ep = r;

    if (!r && (flags & SCODSR)) {
	/* Wait for card present */
	while (!sccardpresent(ttyn))
	    sleep(1);
    }

#ifdef SCPERF
    SetTime ("scopen() end");
#endif /* SCPERF */

    return r ? -1 : ttyn;
}

int
openReader(int readerNum, int flags)
{
    void *libHandle;
    readerInfo *reader;

#ifdef DEBUG
    fprintf(stderr, "openReader %d\n", readerNum);
#endif

    if (readerNum < 0 || readerNum >= numReaders)
	return SCEDRVR;
    reader = &readers[readerNum];

    if (!reader->driverLoaded) {
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

	reader->driverLoaded = 1;
    }

    /* send flags to the driver */
    reader->setcapa(TAG_OPEN_FLAGS, (u_char *)&flags); 
    /* if this returns an error, setcapa is not supported in this driver,
       but that's OK. */

    if (reader->open(reader->channelID))
	return SCECOMM;
    else
	return 0;
}

int
scclose(int readerNum)
{
    readerInfo *reader = &readers[readerNum];

    if (reader->driverLoaded == 0)
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
    
    if (reader->driverLoaded == 0) {
	r = SCECLOSED;
	goto out;
    }

    if (!sccardpresent(ttyn)) {
	r = SCENOCARD;
	goto out;
    }

    /* send flags to the driver */
    reader->setcapa(TAG_RESET_FLAGS, (u_char *)&flags);
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

    /* Ihis does not free scperf.  It looks like memory leak ...
       and it is, but it is actually the right behavior.
       print_time() will print the messages later, so the buffer
       must be there. */
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

    /* Ihis does not free scperf.  It looks like memory leak ...
       and it is, but it is actually the right behavior.
       print_time() will print the messages later, so the buffer
       must be there. */
#endif /* SCPERF */
    rv = scrw(ttyn, cla, ins, p1, p2, 0, NULL, p3, buf, sw1p, sw2p);

#ifdef SCPERF
    SetTime("scread() end");
#endif /* SCPERF */
    return rv; 
}

void
addReader(char *name, unsigned long channelID, char *driverFile)
{
    readerInfo *reader;

    if (numReaders >= MAX_READERS)
	return;

    reader = &readers[numReaders++];

    reader->channelID = channelID;
    reader->driverPath = strdup(driverFile);
    reader->driverLoaded = 0;
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
