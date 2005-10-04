/*	$OpenBSD: theo.c,v 1.89 2005/10/04 02:42:03 marco Exp $	*/
/*
 * Copyright (c) 2002 Artur Grabowski <art@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "def.h"
#include "kbd.h"
#include "funmap.h"

void		theo_init(void);
static int	theo_analyze(int, int);
static int	theo(int, int);

static PF theo_pf[] = {
	theo_analyze
};

static struct KEYMAPE (1 + IMAPEXT) theomap = {
	1,
	1 + IMAPEXT,
	rescan,
	{
		{ CCHR('M'), CCHR('M'), theo_pf, NULL }
	}
};

static BUFFER *tbuf;

void
theo_init(void)
{
	funmap_add(theo, "theo");
	maps_add((KEYMAP *)&theomap, "theo");
}

static int
theo(int f, int n)
{
	BUFFER	*bp;
	MGWIN	*wp;

	bp = bfind("theo", TRUE);
	if (bclear(bp) != TRUE)
		return (FALSE);

	bp->b_modes[0] = name_mode("fundamental");
	bp->b_modes[1] = name_mode("theo");
	bp->b_nmodes = 1;

	if ((wp = popbuf(bp)) == NULL)
		return (FALSE);

	tbuf = curbp = bp;
	curwp = wp;
	return (TRUE);
}

static const char *talk[] = {
	"Write more code.",
	"Make more commits.",
	"That's because you have been slacking.",
	"slacker!",
	"That's what happens when you're lazy.",
	"idler!",
	"slackass!",
	"lazy bum!",
	"Stop slacking you lazy bum!",
	"slacker slacker lazy bum bum bum slacker!",
	"I could search... but I'm a lazy bum ;)",
	"sshutup sshithead, ssharpsshooting susshi sshplats ssharking assholes.",
	"Lazy bums slacking on your asses.",
	"35 commits an hour? That's pathetic!",
	"Fine software takes time to prepare.  Give a little slack.",
	"emacs on the vax",
	"Just a minute ago we were hugging and now you, guys, do not love me anymore",
	"I'll let you know when I need to floss my teeth",
	"If you can't figure out yourself, you're lacking some mental faculties",
	"I am just stating a fact",
	"blah blah",
	"i'd love to hack, but i can't",
	"Wait, yes, I am on drugs",
	"during release it is a constant.  almost noone helps.",
	"i let you guys do whatever you wanted",
	"you bring new meaning to the terms slackass. I will have to invent a new term.",
	"if they cut you out, muddy their back yards",
	"Make them want to start over, and play nice the next time.",
	"It is clear that this has not been thought through.",
	"avoid using abort().  it is not nice.",
	"if you do not test that, you are banned from editing theo.c",
	"That's the most ridiculous thing I've heard in the last two or three minutes!",
	"I'm not just doing this for crowd response. I need to be right.",
	"i admit you are better than i am...",
	"I'd put a fan on my bomb.. And blinking lights...",
	"I love to fight",
	"I am not concerned with commit count",
	"No sane people allowed here.  Go home.",
	"you have to stop peeing on your breakfast",
	"feature requests come from idiots",
	"henning and darren / sitting in a tree / t o k i n g / a joint or three",
	"KICK ASS. TIME FOR A JASON LOVE IN!  WE CAN ALL GET LOST IN HIS HAIR!",
	"shame on you for following my rules.",
	"altq's parser sucks dead whale farts through the finest chemistry pipette's",
	"screw this operating system shit, i just want to drive!",
	"That is the most stupid thing I have heard all week.",
	"Search for fuck.  Anytime you see that word, you have a paragraph to write.",
	"what I'm doing [...] is hell. it's kind of fun.",
	"Yes, but the ports people are into S&M.",
	"Buttons are for idiots.",
	"We are not hackers. We are turd polishing craftsmen.",
	"if ya break cvs, we hunt ya and break yer legs",
	"who cares.  style(9) can bite my ass",
	"The argument is totally Linux.",
	"It'd be one fucking happy planet if it wasn't for what's under this fucking sticker.",
	"noone is gonna add that thing to theo.c?  wow, i'm stunned.  no henning?",
	"I would explain, but I am too drunk.",
	"Take a picture of my butt, it's what everyone wants.",
	"you slackers don't deserve pictures yet",
	"You guys are about four days behind on theo.c",
	"I'm just talking.",
	"Vegetarian my ass",
	"Wait a minute, that's a McNally's!",
	"Your connection is breaking up.",
	"germans are not allowed to get involved there",
	"gprs sucks camel dick dryer than the gobi desert",
	"I AM NEVER SATISFIED",
	"don't they recognize their moral responsibility to entertain me?",
	"#ifdef is for emacs developers.",
	"Many well known people become net-kooks in their later life, because they lose touch with reality.",
	"You're not allowed to have an opinion.",
	"tweep tweep tweep",
	"Quite frankly, SSE's alignment requirement is the most utterly retarded idea since eating your own shit.",
	"Holy verbose prom startup Batman.",
	"Do you think you are exempt from COMPILING BEFORE YOU COMMIT",
	"I want to be REALLY COOL just like all the other developers!",
	"I don't know what you are talking about.  Please tell me what you are talking about.",
	"I avoid helping people who refuse to learn how to help themselves.",
	"Any day now, when we sell out.",
	"And there you have it.. the distinction between those people who are OpenBSD develepers and those who will never be able to be...",
	"I don't mean this applies to everyone, but is there a high quantity of attention deficit disorder in our user community?  Or retards?",
	"Have you been living in a cave, or are you just being a jerk?",
	"If you want it, go do the stinking work yourself.",
	"A burp means less gas later",
	"optimism in man kind does not belong here",
	"First user who tries to push this button, he pounds into the ground with a rant of death.",
	"we did farts.  now we do sperm.  we are cutting edge.",
	"the default configuration is a mixture of piss, puke, shit, and bloody entrails.",
	"Stop wasting your time reading people's licenses.",
	"doing it with environment variables is OH SO SYSTEM FIVE LIKE OH MY GOD PASS ME THE SPOON",
	"Linux is fucking POO, not just bad, bad REALLY REALLY BAD",
	"openbsd development is slow because lots of developers have shrunken balls",
	"penguins are not much more than chickens that swim.",
	"i am a packet sniffing fool, let me wipe my face with my own poo",
	"they are manual pages, not tea time chit-chats",
	"Whiners.  They scale really well.",
	"in your world, you would have a checklist of 50 fucking workarounds just to make a coffee.",
	"for once, I have nothing to say.",
	"You have no idea how fucked we are",
	"You can call it fart if you want to.",
	"wavelan is a battle field",
	"If you don't know what you are talking about, why are you talking?"
};

static const int ntalk = sizeof(talk)/sizeof(talk[0]);

static int
theo_analyze(int f, int n)
{
	const char	*str;
	int		 len;

	str = talk[arc4random() % ntalk];
	len = strlen(str);

	newline(FFRAND, 2);

	while (len--)
		linsert(1, *str++);

	newline(FFRAND, 2);

	return (TRUE);
}
