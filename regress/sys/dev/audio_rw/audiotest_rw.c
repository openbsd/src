/*	$OpenBSD: audiotest_rw.c,v 1.6 2007/08/26 08:37:28 jakemsr Exp $	*/

/*
 * Copyright (c) 2007 Jacob Meuser <jakemsr@sdf.lonestar.org>
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


#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/audioio.h>
#include <sys/mman.h>
#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

extern char *__progname;

void useage(void);
int audio_set_duplex(int, char *, int);
int audio_set_info(int, u_int, u_int, u_int, u_int, u_int);
int audio_trigger_record(int);
int audio_wait_frame(int, u_int, int, int);
int audio_do_frame(int, size_t , char *, char *, u_int, int, int);
int audio_do_test(int, size_t, char *, char *, u_int, int, int, int, int);

void
useage(void)
{
	fprintf(stderr,
	    "usage: %s [-dpsv] [-b buffersize] [-c channels] [-e encoding]\n"
	    "          [-f device] [-i input] [-l loops] [-o output] [-r samplerate]\n",
	    __progname);
	return;
}


int
audio_set_duplex(int audio_fd, char *audio_device, int use_duplex)
{
int i, has_duplex;

	if (ioctl(audio_fd, AUDIO_GETPROPS, &i) < 0) {
		warn("AUDIO_GETPROPS");
		return 1;
	}

	has_duplex = i & AUDIO_PROP_FULLDUPLEX ? 1 : 0;

	if (use_duplex && !has_duplex) {
		warn("%s doesn't support full-duplex", audio_device);
		return 1;
	}

	if (ioctl(audio_fd, AUDIO_SETFD, &use_duplex) < 0) {
		warn("AUDIO_SETFD");
		return 1;
	}

	if (ioctl(audio_fd, AUDIO_GETFD, &i) < 0) {
		warn("AUDIO_GETFD");
		return 1;
	}

	if (i != use_duplex)
		return 1;

	return 0;
}


int
audio_set_info(int audio_fd, u_int mode, u_int encoding, u_int sample_rate,
    u_int channels, u_int buffer_size)
{
audio_info_t audio_if;
audio_encoding_t audio_enc;
u_int precision;

	audio_enc.index = encoding;
	if (ioctl(audio_fd, AUDIO_GETENC, &audio_enc) < 0) {
		warn("AUDIO_GETENC");
		return 1;
	}

	precision = audio_enc.precision;
	encoding = audio_enc.encoding;

	if (encoding == AUDIO_ENCODING_ULINEAR)
		encoding = (BYTE_ORDER == LITTLE_ENDIAN) ?
		    AUDIO_ENCODING_ULINEAR_LE : AUDIO_ENCODING_ULINEAR_BE;

	if (encoding == AUDIO_ENCODING_SLINEAR)
		encoding = (BYTE_ORDER == LITTLE_ENDIAN) ?
		    AUDIO_ENCODING_SLINEAR_LE : AUDIO_ENCODING_SLINEAR_BE;

	AUDIO_INITINFO(&audio_if);

	audio_if.mode = mode;
	audio_if.blocksize = buffer_size;

	if (mode & AUMODE_RECORD) {
		audio_if.record.precision = precision;
		audio_if.record.channels = channels;
		audio_if.record.sample_rate = sample_rate;
		audio_if.record.encoding = encoding;
	}
	if (mode & AUMODE_PLAY) {
		audio_if.play.precision = precision;
		audio_if.play.channels = channels;
		audio_if.play.sample_rate = sample_rate;
		audio_if.play.encoding = encoding;
	}

	if (ioctl(audio_fd, AUDIO_SETINFO, &audio_if) < 0) {
		warn("AUDIO_SETINFO");
		return 1;
	}

	if (ioctl(audio_fd, AUDIO_GETINFO, &audio_if) < 0) {
		warn("AUDIO_GETINFO");
		return 1;
	}

	if (mode & AUMODE_RECORD) {
		if (audio_if.record.precision != precision) {
			warnx("unable to set record precision: tried %u, got %u",
			    precision, audio_if.record.precision);
			return 1;
		}
		if (audio_if.record.channels != channels){
			warnx("unable to set record channels: tried %u, got %u",
			    channels, audio_if.record.channels);
			return 1;
		}
		if (audio_if.record.sample_rate != sample_rate) {
			warnx("unable to set record sample_rate: tried %u, got %u",
			    sample_rate, audio_if.record.sample_rate);
			return 1;
		}
		if (audio_if.record.encoding != encoding) {
			warnx("unable to set record encoding: tried %u, got %u",
			    encoding, audio_if.record.encoding);
			return 1;
		}
	}

	if (mode & AUMODE_PLAY) {
		if (audio_if.play.precision != precision) {
			warnx("unable to set play precision: tried %u, got %u",
			    precision, audio_if.play.precision);
			return 1;
		}
		if (audio_if.play.channels != channels) {
			warnx("unable to set play channels: tried %u, got %u",
			    channels, audio_if.play.channels);
			return 1;
		}
		if (audio_if.play.sample_rate != sample_rate) {
			warnx("unable to set play sample_rate: tried %u, got %u",
			    sample_rate, audio_if.play.sample_rate);
			return 1;
		}
		if (audio_if.play.encoding != encoding) {
			warnx("unable to set play encoding: tried %u, got %u",
			    encoding, audio_if.play.encoding);
			return 1;
		}
	}

	return 0;
}

int
audio_trigger_record(int audio_fd)
{
audio_info_t audio_if;

	AUDIO_INITINFO(&audio_if);
	audio_if.record.pause = 0;
	if (ioctl(audio_fd, AUDIO_SETINFO, &audio_if) < 0) {
		warn("AUDIO_SETINFO: audio_if.record.pause = %d",
		    audio_if.record.pause);
		return 1;
	}

	return 0;
}

/* return 0 on error, 1 if read, 2 if write, 3 if both read and write */
int
audio_wait_frame(int audio_fd, u_int mode, int use_select, int use_poll)
{
struct pollfd pfd[1];
fd_set *sfdsr;
fd_set *sfdsw;
struct timeval tv;
int nfds, max;
int ret;

	ret = 0;

	if (use_select) {
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		max = audio_fd;
		sfdsr = NULL;
		sfdsw = NULL;
		if (mode & AUMODE_RECORD) {
			if ((sfdsr = calloc(max + 1, sizeof(fd_mask))) == NULL) {
				warn("fd_set sfdsr");
				return 0;
			}
			FD_ZERO(sfdsr);
			FD_SET(audio_fd, sfdsr);
		}
		if (mode & AUMODE_PLAY) {
			if ((sfdsw = calloc(max + 1, sizeof(fd_mask))) == NULL) {
				warn("fd_set sfdsw");
				return 0;
			}
			FD_ZERO(sfdsw);
			FD_SET(audio_fd, sfdsw);
		}
		nfds = select(max + 1, sfdsr, sfdsw, NULL, &tv);
		if (nfds == -1) {
			warn("select() error");
			return 0;
		}
		if (nfds == 0) {
			warnx("select() timed out");
			return 0;
		}
		if (mode & AUMODE_RECORD)
			if (FD_ISSET(audio_fd, sfdsr))
				ret |= 1;
		if (mode & AUMODE_PLAY)
			if (FD_ISSET(audio_fd, sfdsw))
				ret |= 2;
		if (sfdsr != NULL)
			free(sfdsr);
		if (sfdsw != NULL)
			free(sfdsw);
	} else if (use_poll) {
		bzero(&pfd[0], sizeof(struct pollfd));
		pfd[0].fd = audio_fd;
		if (mode & AUMODE_RECORD)
			pfd[0].events |= POLLIN;
		if (mode & AUMODE_PLAY)
			pfd[0].events |= POLLOUT;
		nfds = poll(pfd, 1, 1000);
		if (nfds == -1 || (pfd[0].revents & (POLLERR|POLLHUP|POLLNVAL))) {
			warn("poll() error");
			return 0;
		}
		if (nfds == 0) {
			warnx("poll() timed out");
			return 0;
		}
		if (mode & AUMODE_RECORD)
			if (pfd[0].revents & POLLIN)
				ret |= 1;
		if (mode & AUMODE_PLAY)
			if (pfd[0].revents & POLLOUT)
				ret |= 2;
	} else {
		if (mode & AUMODE_RECORD)
			ret |= 1;
		if (mode & AUMODE_PLAY)
			ret |= 2;
	}

	return ret;
}


/* return 0 on error, 1 if read, 2 if write, 3 if both read and write */
int
audio_do_frame(int audio_fd, size_t buffer_size, char *rbuffer, char *wbuffer,
    u_int mode, int use_poll, int use_select)
{
size_t offset;
size_t left;
ssize_t retval;
int ret;

	ret = audio_wait_frame(audio_fd, mode, use_select, use_poll);
	if (ret == 0)
		return 0;

	if (ret & 1) {
		for (left = buffer_size, offset = 0; left > 0;) {
			retval = read(audio_fd, rbuffer + offset, left);
			if (retval == 0)
				warnx("read audio device 0 bytes");
			if (retval < 0) {
				warn("read audio device");
					return 0;
			}
			if (retval > left) {
				warnx("read returns more than requested: "
				    "%ld > %ld", retval, left);
				return 0;
			}
			offset += retval;
			left -= retval;
		}
	}

	if (ret & 2) {
		for (left = buffer_size, offset = 0; left > 0;) {
			retval = write(audio_fd, wbuffer + offset, left);
			if (retval == 0)
				warnx("write audio device 0 bytes");
			if (retval < 0) {
				warn("write audio device");
					return 0;
			}
			if (retval > left) {
				warnx("write returns more than requested: "
				    "%ld > %ld", retval, left);
				return 0;
			}
			offset += retval;
			left -= retval;
		}
	}

	return ret;
}


int
audio_do_test(int audio_fd, size_t buffer_size, char *input_file, char *output_file,
    u_int mode, int use_poll, int use_select, int loops, int verbose)
{
FILE *fout;
FILE *fin;
char *rbuffer;
char *wbuffer;
int buffs_read, buffs_written;
int i, ret;

	fin = NULL;
	fout = NULL;
	rbuffer = NULL;
	wbuffer = NULL;

	if ((rbuffer = malloc(buffer_size)) == NULL)
		err(1, "malloc %lu bytes", (unsigned long)buffer_size);

	if ((wbuffer = malloc(buffer_size)) == NULL)
		err(1, "malloc %lu bytes", (unsigned long)buffer_size);

	if (output_file != NULL) {
		if ((fout = fopen(output_file, "w")) == NULL)
			err(1, "fopen %s", output_file);
	}
	if (input_file != NULL) {
		if ((fin = fopen(input_file, "r")) == NULL)
			err(1, "fopen %s", input_file);
	}

	buffs_read = 0;
	buffs_written = 0;
	if (input_file != NULL) {
		if (fread(wbuffer, buffer_size, 1, fin) < 1) {
			warnx("fread error: %s", input_file);
			return 1;
		}
	}
	for (i = 1; mode && i <= loops; i++) {
		ret = audio_do_frame(audio_fd, buffer_size, rbuffer,
		    wbuffer, mode, use_poll, use_select);
		if (ret == 0)
			return 1;
		if (ret & 1) {
			buffs_read++;
			if (verbose)
				warnx("loop %03d: read frame: %03d", i, buffs_read);
			if (fwrite(rbuffer, buffer_size, 1, fout) < 1) {
				warnx("fwrite error: %s", output_file);
				return 1;
			}
		}
		if (ret & 2) {
			buffs_written++;
			if (verbose)
				warnx("loop %03d: write frame: %03d", i, buffs_written);
			if (fread(wbuffer, buffer_size, 1, fin) < 1) {
				if (feof(fin)) {
					if (verbose)
						warnx("input EOF");
					mode = mode & ~AUMODE_PLAY;
				} else {
					warnx("fread error: %s", input_file);
					return 1;
				}
			}
		}
	}

	if (output_file != NULL)
		if (fileno(fout) >= 0)
			fclose(fout);
	if (input_file != NULL)
		if (fileno(fin) >= 0)
			fclose(fin);

	if (rbuffer != NULL)
		free(rbuffer);
	if (wbuffer != NULL)
		free(wbuffer);

	return 0;
}


int
main(int argc, char *argv[])
{
char *audio_device;
char *output_file;
char *input_file;
int audio_fd;
size_t buffer_size;

audio_device_t audio_dev;
audio_info_t audio_if;
u_int sample_rate;
u_int channels;
u_int mode;
u_int encoding;

int flags;
int use_duplex;
int use_nonblock;
int use_poll;
int use_select;
int verbose;

int loops;

const char *errstr;

int ch;
extern char *optarg;
extern int optind;


	audio_device = "/dev/audio";
	input_file = NULL;
	output_file = NULL;

	audio_fd = -1;

	buffer_size = 8192;
	sample_rate = 48000;
	channels = 2;

	encoding = 0;

	loops = 64;
	use_nonblock = 0;
	use_select = 0;
	use_poll = 0;
	use_duplex = 0;
	verbose = 0;

	while ((ch = getopt(argc, argv, "b:c:e:f:i:l:o:r:dpsv")) != -1) {
		switch (ch) {
		case 'b':
			buffer_size = (size_t)strtonum(optarg, 32, 65536, &errstr);
			if (errstr != NULL)
				errx(1, "could not grok buffer_size: %s", errstr);
			break;
		case 'c':
			channels = (u_int)strtonum(optarg, 1, 2, &errstr);
			if (errstr != NULL)
				errx(1, "could not grok channels: %s", errstr);
			break;
		case 'd':
			use_duplex = 1;
			break;
		case 'e':
			encoding = (u_int)strtonum(optarg, 0, 24, &errstr);
			if (errstr != NULL)
				errx(1, "could not grok encoding: %s", errstr);
			break;
		case 'f':
			audio_device = optarg;
			break;
		case 'i':
			input_file = optarg;
			break;
		case 'l':
			loops = (int)strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr != NULL)
				errx(1, "could not grok loops: %s", errstr);
			break;
		case 'o':
			output_file = optarg;
			break;
		case 'p':
			use_poll = 1;
			use_nonblock = 1;
			break;
		case 'r':
			sample_rate = (u_int)strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr != NULL)
				errx(1, "could not grok sample_rate: %s", errstr);
			break;
		case 's':
			use_select = 1;
			use_nonblock = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			useage();
			exit(1);
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (use_select && use_poll)
		errx(1, "can't use select and poll at the same time");

	if ((input_file == NULL) && (output_file == NULL))
		errx(1, "no input or output file specified");

	if ((input_file != NULL) && (output_file != NULL))
		use_duplex = 1;

	mode = 0;
	flags = 0;

	if (output_file != NULL) {
		mode |= AUMODE_RECORD;
		flags = O_RDONLY;
	}

	if (input_file != NULL) {
		mode |= AUMODE_PLAY;
		flags = O_WRONLY;
	}

	if (use_duplex)
		flags = O_RDWR;

	if (use_nonblock)
		flags |= O_NONBLOCK;

	if ((audio_fd = open(audio_device, flags)) < 0)
		err(1, "open %s", audio_device);

	if (audio_set_duplex(audio_fd, audio_device, use_duplex))
		errx(1, "could not set duplex mode");

	if (audio_set_info(audio_fd, mode, encoding, sample_rate, channels,
	    (u_int)buffer_size))
		errx(1, "could not initialize audio device");

	if (verbose) {
		AUDIO_INITINFO(&audio_if);
		if (ioctl(audio_fd, AUDIO_GETINFO, &audio_if) < 0)
			err(1, "AUDIO_GETINFO");

		if (ioctl(audio_fd, AUDIO_GETDEV, &audio_dev) < 0)
			err(1, "AUDIO_GETDEV");

		warnx("audio device:  %s: %s ver %s, config: %s", audio_device,
		    audio_dev.name, audio_dev.version, audio_dev.config);
		warnx("blocksize:          %u", audio_if.blocksize);
		warnx("lowat:              %u", audio_if.lowat);
		warnx("hiwat:              %u", audio_if.hiwat);
		warnx("play.buffer_size:   %u", audio_if.play.buffer_size); 
		warnx("record.buffer_size: %u", audio_if.record.buffer_size); 
		if (output_file != NULL)
			warnx("output file:        %s", output_file);
		if (input_file != NULL)
			warnx("input file:         %s", input_file);
		warnx("flags:              %d", flags);
		warnx("mode:               %u", mode);
		warnx("encoding:           %u", encoding);
		warnx("sample_rate:        %u", sample_rate);
		warnx("channels:           %u", channels);
		warnx("use_select:         %d", use_select);
		warnx("use_poll:           %d", use_poll);
		warnx("use_duplex:         %d", use_duplex);
		warnx("buffer_size:        %lu", (unsigned long)buffer_size);
	}

	/* need to trigger recording in duplex mode */
	if (use_duplex && (mode & AUMODE_RECORD))
		if (audio_trigger_record(audio_fd))
			exit(1);

	if (audio_do_test(audio_fd, buffer_size, input_file, output_file,
	    mode, use_poll, use_select, loops, verbose))
		exit(1);

	if (verbose)
		warnx("test completed");

	if (audio_fd >= 0)
		close(audio_fd);

	exit(0);
}
