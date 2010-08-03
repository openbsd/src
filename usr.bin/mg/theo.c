/*	$OpenBSD: theo.c,v 1.120 2010/08/03 22:12:27 henning Exp $	*/
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

void
theo_init(void)
{
	funmap_add(theo, "theo");
	maps_add((KEYMAP *)&theomap, "theo");
}

/* ARGSUSED */
static int
theo(int f, int n)
{
	struct buffer	*bp;
	struct mgwin	*wp;

	bp = bfind("theo", TRUE);
	if (bclear(bp) != TRUE)
		return (FALSE);

	bp->b_modes[0] = name_mode("fundamental");
	bp->b_modes[1] = name_mode("theo");
	bp->b_nmodes = 1;

	if ((wp = popbuf(bp, WNONE)) == NULL)
		return (FALSE);

	curbp = bp;
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
	"I am just stating a fact",
	"you bring new meaning to the terms slackass. I will have to invent a new term.",
	"if they cut you out, muddy their back yards",
	"Make them want to start over, and play nice the next time.",
	"It is clear that this has not been thought through.",
	"avoid using abort().  it is not nice.",
	"That's the most ridiculous thing I've heard in the last two or three minutes!",
	"I'm not just doing this for crowd response. I need to be right.",
	"I'd put a fan on my bomb.. And blinking lights...",
	"I love to fight",
	"No sane people allowed here.  Go home.",
	"you have to stop peeing on your breakfast",
	"feature requests come from idiots",
	"henning and darren / sitting in a tree / t o k i n g / a joint or three",
	"KICK ASS. TIME FOR A JASON LOVE IN!  WE CAN ALL GET LOST IN HIS HAIR!",
	"shame on you for following my rules.",
	"altq's parser sucks dead whale farts through the finest chemistry pipette's",
	"screw this operating system shit, i just want to drive!",
	"Search for fuck.  Anytime you see that word, you have a paragraph to write.",
	"Yes, but the ports people are into S&M.",
	"Buttons are for idiots.",
	"We are not hackers. We are turd polishing craftsmen.",
	"who cares.  style(9) can bite my ass",
	"It'd be one fucking happy planet if it wasn't for what's under this fucking sticker.",
	"I would explain, but I am too drunk.",
	"you slackers don't deserve pictures yet",
	"Vegetarian my ass",
	"Wait a minute, that's a McNally's!",
	"don't they recognize their moral responsibility to entertain me?",
	"#ifdef is for emacs developers.",
	"Many well known people become net-kooks in their later life, because they lose touch with reality.",
	"You're not allowed to have an opinion.",
	"tweep tweep tweep",
	"Quite frankly, SSE's alignment requirement is the most utterly retarded idea since eating your own shit.",
	"Holy verbose prom startup Batman.",
	"Any day now, when we sell out.",
	"optimism in man kind does not belong here",
	"First user who tries to push this button, he pounds into the ground with a rant of death.",
	"we did farts.  now we do sperm.  we are cutting edge.",
	"the default configuration is a mixture of piss, puke, shit, and bloody entrails.",
	"Stop wasting your time reading people's licenses.",
	"doing it with environment variables is OH SO SYSTEM FIVE LIKE OH MY GOD PASS ME THE SPOON",
	"Linux is fucking POO, not just bad, bad REALLY REALLY BAD",
	"penguins are not much more than chickens that swim.",
	"i am a packet sniffing fool, let me wipe my face with my own poo",
	"Whiners.  They scale really well.",
	"in your world, you would have a checklist of 50 fucking workarounds just to make a coffee.",
	"for once, I have nothing to say.",
	"You have no idea how fucked we are",
	"You can call it fart if you want to.",
	"wavelan is a battle field",
	"You are in a maze of gpio pins, all alike, all undocumented, and a few are wired to bombs.",
	"And that is why humppa sucks... cause it has no cause.",
	"cache aliasing is a problem that would have stopped in 1992 if someone had killed about 5 people who worked at Sun.",
	"Don't spread rumours about me being gentle.",
	"If municipal water filtering equipment was built by the gcc developers, the western world would be dead by now.",
	"kettenis supported a new machine in my basement and all I got to do was fix a 1 character typo in his html page commit.",
	"industry told us a lesson: when you're an asshole, they mail you hardware",
	"I was joking, really.  I think I am funny :-)",
	"the kernel is a harsh mistress",
	"Have I ever been subtle? If my approach ever becomes subtle, shoot me.",
	"the acpi stabs you in the back.  the acpi stabs you in the back. you die ...",
	"My cats are more observant than you.",
	"our kernels have no bugs",
	"style(9) has all these fascist rules, and i have a problem with some of them because i didn't come up with them",
	"I'm not very reliable",
	"I don't like control",
	"You aren't being conservative -- you are trying to be a caveman.",
	"nfs loves everyone",
	"basically, dung beetles fucking.  that's what kerberosV + openssl is like",
	"I would rather run Windows than use vi."
};

static const int ntalk = sizeof(talk)/sizeof(talk[0]);

/* ARGSUSED */
static int
theo_analyze(int f, int n)
{
	const char	*str;
	int		 len;

	str = talk[arc4random_uniform(ntalk)];
	len = strlen(str);

	newline(FFRAND, 2);

	while (len--)
		linsert(1, *str++);

	newline(FFRAND, 2);

	return (TRUE);
}
