/*	$OpenBSD: audioctl.c,v 1.7 2002/12/10 09:03:37 pvalchev Exp $	*/
/*	$NetBSD: audioctl.c,v 1.14 1998/04/27 16:55:23 augustss Exp $	*/

/*
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Author: Lennart Augustsson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * audioctl(1) - a program to control audio device.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <err.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/audioio.h>

struct field *findfield(char *name);
void prfield(struct field *p, char *sep);
void rdfield(struct field *p, char *q);
void getinfo(int fd);
void usage(void);
int main(int argc, char **argv);

FILE *out = stdout;

audio_device_t adev;

audio_info_t info;

char encbuf[1000];

int properties, fullduplex, rerror;

struct field {
	char *name;
	void *valp;
	int format;
#define STRING 1
#define INT 2
#define UINT 3
#define P_R 4
#define ULONG 5
#define UCHAR 6
#define ENC 7
#define PROPS 8
#define XINT 9
	char flags;
#define READONLY 1
#define ALIAS 2
#define SET 4
} fields[] = {
	{ "name", 		&adev.name, 		STRING, READONLY },
	{ "version",		&adev.version,		STRING, READONLY },
	{ "config",		&adev.config,		STRING, READONLY },
	{ "encodings",		encbuf,			STRING, READONLY },
	{ "properties",		&properties,		PROPS,	READONLY },
	{ "full_duplex",	&fullduplex,		UINT,	0 },
	{ "fullduplex",		&fullduplex,		UINT,	0 },
	{ "blocksize",		&info.blocksize,	UINT,	0 },
	{ "hiwat",		&info.hiwat,		UINT,	0 },
	{ "lowat",		&info.lowat,		UINT,	0 },
	{ "monitor_gain",	&info.monitor_gain,	UINT,	0 },
	{ "mode",		&info.mode,		P_R,	READONLY },
	{ "play.rate",		&info.play.sample_rate,	UINT,	0 },
	{ "play.sample_rate",	&info.play.sample_rate,	UINT,	ALIAS },
	{ "play.channels",	&info.play.channels,	UINT,	0 },
	{ "play.precision",	&info.play.precision,	UINT,	0 },
	{ "play.encoding",	&info.play.encoding,	ENC,	0 },
	{ "play.gain",		&info.play.gain,	UINT,	0 },
	{ "play.balance",	&info.play.balance,	UCHAR,	0 },
	{ "play.port",		&info.play.port,	XINT,	0 },
	{ "play.avail_ports",	&info.play.avail_ports,	XINT,	0 },
	{ "play.seek",		&info.play.seek,	ULONG,	READONLY },
	{ "play.samples",	&info.play.samples,	UINT,	READONLY },
	{ "play.eof",		&info.play.eof,		UINT,	READONLY },
	{ "play.pause",		&info.play.pause,	UCHAR,	0 },
	{ "play.error",		&info.play.error,	UCHAR,	READONLY },
	{ "play.waiting",	&info.play.waiting,	UCHAR,	READONLY },
	{ "play.open",		&info.play.open,	UCHAR,	READONLY },
	{ "play.active",	&info.play.active,	UCHAR,	READONLY },
	{ "play.buffer_size",	&info.play.buffer_size,	UINT,	0 },
	{ "record.rate",	&info.record.sample_rate,UINT,	0 },
	{ "record.sample_rate",	&info.record.sample_rate,UINT,	ALIAS },
	{ "record.channels",	&info.record.channels,	UINT,	0 },
	{ "record.precision",	&info.record.precision,	UINT,	0 },
	{ "record.encoding",	&info.record.encoding,	ENC,	0 },
	{ "record.gain",	&info.record.gain,	UINT,	0 },
	{ "record.balance",	&info.record.balance,	UCHAR,	0 },
	{ "record.port",	&info.record.port,	XINT,	0 },
	{ "record.avail_ports",	&info.record.avail_ports,XINT,	0 },
	{ "record.seek",	&info.record.seek,	ULONG,	READONLY },
	{ "record.samples",	&info.record.samples,	UINT,	READONLY },
	{ "record.eof",		&info.record.eof,	UINT,	READONLY },
	{ "record.pause",	&info.record.pause,	UCHAR,	0 },
	{ "record.error",	&info.record.error,	UCHAR,	READONLY },
	{ "record.waiting",	&info.record.waiting,	UCHAR,	READONLY },
	{ "record.open",	&info.record.open,	UCHAR,	READONLY },
	{ "record.active",	&info.record.active,	UCHAR,	READONLY },
	{ "record.buffer_size",	&info.record.buffer_size,UINT,	0 },
	{ "record.errors",	&rerror,		INT,	READONLY },
	{ 0 }
};

struct {
	char *ename;
	int eno;
} encs[] = {
	{ AudioEmulaw,		AUDIO_ENCODING_ULAW },
	{ "ulaw",		AUDIO_ENCODING_ULAW },
	{ AudioEalaw, 		AUDIO_ENCODING_ALAW },
	{ AudioEslinear,	AUDIO_ENCODING_SLINEAR },
	{ "linear",		AUDIO_ENCODING_SLINEAR },
	{ AudioEulinear,	AUDIO_ENCODING_ULINEAR },
	{ AudioEadpcm,		AUDIO_ENCODING_ADPCM },
	{ "ADPCM",		AUDIO_ENCODING_ADPCM },
	{ AudioEslinear_le,	AUDIO_ENCODING_SLINEAR_LE },
	{ "linear_le",		AUDIO_ENCODING_SLINEAR_LE },
	{ AudioEulinear_le,	AUDIO_ENCODING_ULINEAR_LE },
	{ AudioEslinear_be,	AUDIO_ENCODING_SLINEAR_BE },
	{ "linear_be",		AUDIO_ENCODING_SLINEAR_BE },
	{ AudioEulinear_be,	AUDIO_ENCODING_ULINEAR_BE },
	{ AudioEmpeg_l1_stream,	AUDIO_ENCODING_MPEG_L1_STREAM },
	{ AudioEmpeg_l1_packets,AUDIO_ENCODING_MPEG_L1_PACKETS },
	{ AudioEmpeg_l1_system,	AUDIO_ENCODING_MPEG_L1_SYSTEM },
	{ AudioEmpeg_l2_stream,	AUDIO_ENCODING_MPEG_L2_STREAM },
	{ AudioEmpeg_l2_packets,AUDIO_ENCODING_MPEG_L2_PACKETS },
	{ AudioEmpeg_l2_system,	AUDIO_ENCODING_MPEG_L2_SYSTEM },
	{ 0 }
};

static struct {
	char *name;
	u_int prop;
} props[] = {
	{ "full_duplex",	AUDIO_PROP_FULLDUPLEX },
	{ "mmap",		AUDIO_PROP_MMAP },
	{ "independent",	AUDIO_PROP_INDEPENDENT },
	{ 0 }
};

struct field *
findfield(char *name)
{
	int i;
	for(i = 0; fields[i].name; i++)
		if (strcmp(fields[i].name, name) == 0)
			return &fields[i];
	return (0);
}

void
prfield(struct field *p, char *sep)
{
	u_int v;
	char *cm;
	int i;

	if (sep)
		fprintf(out, "%s%s", p->name, sep);
	switch(p->format) {
	case STRING:
		fprintf(out, "%s", (char*)p->valp);
		break;
	case INT:
		fprintf(out, "%d", *(int*)p->valp);
		break;
	case UINT:
		fprintf(out, "%u", *(u_int*)p->valp);
		break;
	case XINT:
		fprintf(out, "0x%x", *(u_int*)p->valp);
		break;
	case UCHAR:
		fprintf(out, "%u", *(u_char*)p->valp);
		break;
	case ULONG:
		fprintf(out, "%lu", *(u_long*)p->valp);
		break;
	case P_R:
		v = *(u_int*)p->valp;
		cm = "";
		if (v & AUMODE_PLAY) {
			if (v & AUMODE_PLAY_ALL)
				fprintf(out, "play");
			else
				fprintf(out, "playsync");
			cm = ",";
		}
		if (v & AUMODE_RECORD)
			fprintf(out, "%srecord", cm);
		break;
	case ENC:
		v = *(u_int*)p->valp;
		for(i = 0; encs[i].ename; i++)
			if (encs[i].eno == v)
				break;
		if (encs[i].ename)
			fprintf(out, "%s", encs[i].ename);
		else
			fprintf(out, "%u", v);
		break;
	case PROPS:
		v = *(u_int*)p->valp;
		for (cm = "", i = 0; props[i].name; i++) {
			if (v & props[i].prop) {
				fprintf(out, "%s%s", cm, props[i].name);
				cm = ",";
			}
		}
		break;
	default:
		errx(1, "Invalid print format.");
	}
}

void
rdfield(struct field *p, char *q)
{
	int i;
	u_int u;

	switch(p->format) {
	case UINT:
		if (sscanf(q, "%u", (unsigned int *)p->valp) != 1)
			warnx("Bad number %s", q);
		break;
	case UCHAR:
		if (sscanf(q, "%u", &u) != 1)
			warnx("Bad number %s", q);
		else
			*(u_char *)p->valp = u;
		break;
	case XINT:
		if (sscanf(q, "0x%x", (unsigned int *)p->valp) != 1 &&
		    sscanf(q, "%x", (unsigned int *)p->valp) != 1)
			warnx("Bad number %s", q);
		break;
	case ENC:
		for(i = 0; encs[i].ename; i++)
			if (strcmp(encs[i].ename, q) == 0)
				break;
		if (encs[i].ename)
			*(u_int*)p->valp = encs[i].eno;
		else
			warnx("Unknown encoding: %s", q);
		break;
	default:
		errx(1, "Invalid read format.");
	}
	p->flags |= SET;
}

void
getinfo(int fd)
{
	int pos = 0, i = 0;

	if (ioctl(fd, AUDIO_GETDEV, &adev) < 0)
		err(1, "AUDIO_GETDEV");
	for(;;) {
	       audio_encoding_t enc;
	       enc.index = i++;
	       if (ioctl(fd, AUDIO_GETENC, &enc) < 0)
		       break;
	       if (pos)
		       encbuf[pos++] = ',';
	       pos += snprintf(encbuf+pos, sizeof(encbuf)-pos, "%s:%d%s",
			       enc.name, enc.precision,
	            	       enc.flags & AUDIO_ENCODINGFLAG_EMULATED ? "*" : "");
	}
	if (ioctl(fd, AUDIO_GETFD, &fullduplex) < 0)
		err(1, "AUDIO_GETFD");
	if (ioctl(fd, AUDIO_GETPROPS, &properties) < 0)
		err(1, "AUDIO_GETPROPS");
	if (ioctl(fd, AUDIO_RERROR, &rerror) < 0)
		err(1, "AUDIO_RERROR");
	if (ioctl(fd, AUDIO_GETINFO, &info) < 0)
		err(1, "AUDIO_GETINFO");
}

void
usage(void)
{
	extern char *__progname;		/* from crt0.o */

	fprintf(stderr,
	    "usage: %s [-f file] [-n] -a\n"
	    "       %s [-f file] [-n] name [...]\n"
	    "       %s [-f file] [-n] -w name=value [...]\n", __progname,
		__progname, __progname);

	exit(1);
}

int
main(int argc, char **argv)
{
	int fd, i, ch;
	int aflag = 0, wflag = 0;
	struct stat dstat, ostat;
	char *file;
	char *sep = "=";
    
	if ((file = getenv("AUDIOCTLDEVICE")) == 0 || *file == '\0')
		file = "/dev/audioctl";
    
	while ((ch = getopt(argc, argv, "af:nw")) != -1) {
		switch(ch) {
		case 'a':
			aflag++;
			break;
		case 'w':
			wflag++;
			break;
		case 'n':
			sep = 0;
			break;
		case 'f':
			file = optarg;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
    
	if ((fd = open(file, wflag ? O_RDWR : O_RDONLY)) < 0)
		err(1, "%s", file);
    
	/* Check if stdout is the same device as the audio device. */
	if (fstat(fd, &dstat) < 0)
		err(1, "fstat au");
	if (fstat(STDOUT_FILENO, &ostat) < 0)
		err(1, "fstat stdout");
	if (S_ISCHR(dstat.st_mode) && S_ISCHR(ostat.st_mode) &&
	    major(dstat.st_dev) == major(ostat.st_dev) &&
	    minor(dstat.st_dev) == minor(ostat.st_dev))
		/* We can't write to stdout so use stderr */
		out = stderr;

	if (!wflag)
		getinfo(fd);

	if (!argc && aflag && !wflag) {
		for(i = 0; fields[i].name; i++) {
			if (!(fields[i].flags & ALIAS)) {
				prfield(&fields[i], sep);
				fprintf(out, "\n");
			}
		}
	} else if (argc > 0 && !aflag) {
		struct field *p;
		if (wflag) {
			AUDIO_INITINFO(&info);
			while(argc--) {
				char *q;
		
				if (q = strchr(*argv, '=')) {
					*q++ = 0;
					p = findfield(*argv);
					if (p == 0)
						warnx("field `%s' does not exist", *argv);
					else {
						if (p->flags & READONLY)
							warnx("`%s' is read only", *argv);
						else {
							rdfield(p, q);
							if (p->valp == &fullduplex)
								if (ioctl(fd, AUDIO_SETFD, &fullduplex) < 0)
									err(1, "set failed");
						}
					}
				} else
					warnx("No `=' in %s", *argv);
				argv++;
			}
			if (ioctl(fd, AUDIO_SETINFO, &info) < 0)
				err(1, "set failed");
			if (sep) {
				getinfo(fd);
				for(i = 0; fields[i].name; i++) {
					if (fields[i].flags & SET) {
						fprintf(out, "%s: -> ", fields[i].name);
						prfield(&fields[i], 0);
						fprintf(out, "\n");
					}
				}
			}
		} else {
			while(argc--) {
				p = findfield(*argv);
				if (p == 0) {
					if (strchr(*argv, '='))
						warnx("field %s does not exist (use -w to set a variable)", *argv);
					else
						warnx("field %s does not exist", *argv);
				} else {
					prfield(p, sep);
					fprintf(out, "\n");
				}
				argv++;
			}
		}
	} else
		usage();
	exit(0);
}
