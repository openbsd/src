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

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/audioio.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char *optarg;
extern int optind;

int audio_info_set(int);
int test_pr_members(int, int);
int test_main_members(int);
void audio_set_init(void);
void audio_set_test(int);

audio_info_t audio_if_init;
audio_info_t audio_if_get;
audio_info_t audio_if_set;



int
audio_info_set(int audio_fd)
{
	return ioctl(audio_fd, AUDIO_SETINFO, &audio_if_set);
}


void
audio_set_init(void)
{
	AUDIO_INITINFO(&audio_if_init);
	audio_if_set = audio_if_init;
}


void
audio_set_test(int audio_fd)
{
	if (audio_info_set(audio_fd) < 0)
		printf(" <- ERROR\n");
	else
		printf("\n");
}


int
test_pr_members(int audio_fd, int mode)
{
struct audio_prinfo *s, *g;

	if (mode) {
		g = &audio_if_get.play;
		s = &audio_if_set.play;
	} else {
		g = &audio_if_get.record;
		s = &audio_if_set.record;
	}

	printf("%s.sample_rate = %u", (mode ? "play" : "record"), g->sample_rate);
	audio_set_init();
	s->sample_rate = g->sample_rate;
	audio_set_test(audio_fd);

	printf("%s.encoding = %u", (mode ? "play" : "record"), g->encoding);
	audio_set_init();
	s->encoding = g->encoding;
	audio_set_test(audio_fd);

	printf("%s.precision = %u", (mode ? "play" : "record"), g->precision);
	audio_set_init();
	s->precision = g->precision;
	audio_set_test(audio_fd);

	printf("%s.channels = %u", (mode ? "play" : "record"), g->channels);
	audio_set_init();
	s->channels = g->channels;
	audio_set_test(audio_fd);

	printf("%s.port = %u", (mode ? "play" : "record"), g->port);
	audio_set_init();
	s->port = g->port;
	audio_set_test(audio_fd);

	printf("%s.gain = %u", (mode ? "play" : "record"), g->gain);
	audio_set_init();
	s->gain = g->gain;
	audio_set_test(audio_fd);

	printf("%s.balance = %u", (mode ? "play" : "record"), g->balance);
	audio_set_init();
	s->balance = g->balance;
	audio_set_test(audio_fd);

	printf("%s.pause = %u", (mode ? "play" : "record"), g->pause);
	audio_set_init();
	s->pause = g->pause;
	audio_set_test(audio_fd);

	return 0;
}

int
test_main_members(int audio_fd)
{
	printf("mode = %d", audio_if_get.mode);
	audio_set_init();
	audio_if_set.mode = audio_if_get.mode;
	audio_set_test(audio_fd);

	printf("monitor_gain = %d", audio_if_get.monitor_gain);
	audio_set_init();
	audio_if_set.monitor_gain = audio_if_get.monitor_gain;
	audio_set_test(audio_fd);

	printf("blocksize = %d", audio_if_get.blocksize);
	audio_set_init();
	audio_if_set.blocksize = audio_if_get.blocksize;
	audio_set_test(audio_fd);

	printf("hiwat = %d", audio_if_get.hiwat);
	audio_set_init();
	audio_if_set.hiwat = audio_if_get.hiwat;
	audio_set_test(audio_fd);

	printf("lowat = %d", audio_if_get.lowat);
	audio_set_init();
	audio_if_set.lowat = audio_if_get.lowat;
	audio_set_test(audio_fd);

	return 0;
}

int
main(int argc, char *argv[])
{
char *audio_device;
int audio_fd;
int ch;
int exval;

	audio_device = "/dev/audio";

	while ((ch = getopt(argc, argv, "f:")) != -1) {
		switch (ch) {
		case 'f':
			audio_device = optarg;
			break;
		default:
			break;
		}
	}
	argc -= optind;
	argv += optind;

	audio_fd = -1;
	if ((audio_fd = open(audio_device, O_WRONLY)) < 0)
		err(1, "could not open %s", audio_device);

	AUDIO_INITINFO(&audio_if_init);
	AUDIO_INITINFO(&audio_if_get);
	AUDIO_INITINFO(&audio_if_set);

	audio_if_set = audio_if_init;

	if (audio_info_set(audio_fd) < 0)
		err(1, "results will be invalid");

	if (ioctl(audio_fd, AUDIO_GETINFO, &audio_if_get) < 0)
		err(1, "AUDIO_GETINFO audio_if_get");

	exval = 1;

	audio_if_set = audio_if_get;
	if (audio_info_set(audio_fd) < 0)
		warn("AUDIO_SETINFO audio_if_get");
	else {
		exval = 0;
		goto done;
	}

	test_pr_members(audio_fd, 1);  /* play */
	test_pr_members(audio_fd, 0);  /* record */
	test_main_members(audio_fd);

done:

	if (audio_fd != -1)
		close(audio_fd);

	exit(exval);
}
