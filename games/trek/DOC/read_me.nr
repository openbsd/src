.\"	$OpenBSD: read_me.nr,v 1.4 2001/01/08 05:08:43 pjanzen Exp $
.\"	$NetBSD: read_me.nr,v 1.2 1995/04/22 10:59:44 cgd Exp $
.de @h
'sp 4
'tl 'TREK SETUP INSTRUCTIONS''%'
'sp 2
.ns
..
.de @f
'bp
..
.wh 0 @h
.wh -6 @f
.de pp
.sp
.ne 2
.ti +5
..
.de s1
.sp 2
.nr S1 +1
.nr S2 0
.ne 5
.in 4
.ti 0
\\n(S1.\ \ \c
..
.de s2
.sp 1
.nr S2 +1
.ne 3
.in 8
.ti 4
\\n(S2.\ \ \c
..
.br
.ce
TREK SETUP INSTRUCTIONS
.sp 2
.pp
This document describes all sorts of nifty things
you should know
before you start to muck around
with the trek source code.
Please read them carefully.
.s1
MAINTENANCE
.pp
There is a compilation option xTRACE which must be set for any
trace information to be generated
(the -t option must also be set on the command line).
It is no longer defined by default.
.s1
DISCLAIMERS
.s2
Frankly,
I am getting pretty sick of playing this game.
Hence,
the version which you get may have several bugs
in it;
I freely admit
that it is probably buggier
than some previous versions.
Sorry about that.
.s2
Along with being buggy,
the game never had quite everything implemented
that was originally intended.
If you see things that look weird,
that may be why.
There are even some features which I have taken out
(like ghost starsystems)
upon deciding that I didn't have the energy
to implement them correctly.
.s1
REQUESTS
.pp
There are several things that I would like to ask of anyone
who does work on the source code.
.s2
Please let me know of any bugs which you find
in the code,
and any fixes which you may have.
Other copies will probably be going out to other people later,
and it would be nice if those copies where less buggy.
Also,
I would be interested in hearing about any
enhancements of the game which you might install.
.s2
Please note that I have a distinct coding style.
I feel that it is cleaner
and easier to read than a more
casual style.
If possible,
please stick to it,
especially if you end up sending tapes back to me.
This goes along with my whole belief in clean code:
I ask you to please avoid obscure code
whenever possible.
If you throw some in,
please don't let me see it.
It just depresses me.
.s2
Unfortunately,
the game is huge.
There are many neat things
which could go in,
if there were only enough space.
However,
I have specifically not gone to separated I/D
space.
The main reason is that I would like future versions
of the game
to be 11/40 compatible.
.s1
SUGGESTIONS FOR THE FUTURE
.pp
If you happen to have more energy than I do,
you may want to examine the following areas.
These are things that I may get to,
but don't hold your breath.
.s2
The routine "capture" is quite unclean.
First, it should have a manner of selecting Klingons
other than random,
either selecting the most likely
or asking the captain (probably best).
It should either be fully implemented,
which includes adding a "board" routine
(half written,
on some tapes as board.x)
which sends a boarding party to forcefully
take over the Klingon,
or it should go out completely,
which is probably what I will end up doing.
When this happens,
the transporter will go completely.
It seems that the space may be better used
for something which more directly enhances the game.
.sp 3
.in 0
Well, that's about it.
To get hold of me,
write to:
.nf
.sp
Eric P Allman
Electronics Research Laboratory
University of California
Berkeley, California  94720
.fi

Happy trekking!!
.pp
