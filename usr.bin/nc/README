Netcat 1.10
===========							   /\_/\
								  / 0 0 \
Netcat is a simple Unix utility which reads and writes data	 ====v====
across network connections, using TCP or UDP protocol.		  \  W  /
It is designed to be a reliable "back-end" tool that can	  |     |     _
be used directly or easily driven by other programs and		  / ___ \    /
scripts.  At the same time, it is a feature-rich network	 / /   \ \  |
debugging and exploration tool, since it can create almost	(((-----)))-'
any kind of connection you would need and has several		 /
interesting built-in capabilities.  Netcat, or "nc" as the	(      ___
actual program is named, should have been supplied long ago	 \__.=|___E
as another one of those cryptic but standard Unix tools.	        /

In the simplest usage, "nc host port" creates a TCP connection to the given
port on the given target host.  Your standard input is then sent to the host,
and anything that comes back across the connection is sent to your standard
output.  This continues indefinitely, until the network side of the connection
shuts down.  Note that this behavior is different from most other applications
which shut everything down and exit after an end-of-file on the standard input.

Netcat can also function as a server, by listening for inbound connections
on arbitrary ports and then doing the same reading and writing.  With minor
limitations, netcat doesn't really care if it runs in "client" or "server"
mode -- it still shovels data back and forth until there isn't any more left.
In either mode, shutdown can be forced after a configurable time of inactivity
on the network side.

And it can do this via UDP too, so netcat is possibly the "udp telnet-like"
application you always wanted for testing your UDP-mode servers.  UDP, as the
"U" implies, gives less reliable data transmission than TCP connections and
some systems may have trouble sending large amounts of data that way, but it's
still a useful capability to have.

You may be asking "why not just use telnet to connect to arbitrary ports?"
Valid question, and here are some reasons.  Telnet has the "standard input
EOF" problem, so one must introduce calculated delays in driving scripts to
allow network output to finish.  This is the main reason netcat stays running
until the *network* side closes.  Telnet also will not transfer arbitrary
binary data, because certain characters are interpreted as telnet options and
are thus removed from the data stream.  Telnet also emits some of its
diagnostic messages to standard output, where netcat keeps such things
religiously separated from its *output* and will never modify any of the real
data in transit unless you *really* want it to.  And of course telnet is
incapable of listening for inbound connections, or using UDP instead.  Netcat
doesn't have any of these limitations, is much smaller and faster than telnet,
and has many other advantages.

Some of netcat's major features are:

	Outbound or inbound connections, TCP or UDP, to or from any ports
	Full DNS forward/reverse checking, with appropriate warnings
	Ability to use any local source port
	Ability to use any locally-configured network source address
	Built-in port-scanning capabilities, with randomizer
	Built-in loose source-routing capability
	Can read command line arguments from standard input
	Slow-send mode, one line every N seconds
	Hex dump of transmitted and received data
	Optional ability to let another program service established connections
	Optional telnet-options responder

Efforts have been made to have netcat "do the right thing" in all its various
modes.  If you believe that it is doing the wrong thing under whatever
circumstances, please notify me and tell me how you think it should behave.
If netcat is not able to do some task you think up, minor tweaks to the code
will probably fix that.  It provides a basic and easily-modified template for
writing other network applications, and I certainly encourage people to make
custom mods and send in any improvements they make to it.  This is the second
release; the overall differences from 1.00 are relatively minor and have mostly
to do with portability and bugfixes.  Many people provided greatly appreciated
fixes and comments on the 1.00 release.  Continued feedback from the Internet
community is always welcome!

Netcat is entirely my own creation, although plenty of other code was used as
examples.  It is freely given away to the Internet community in the hope that
it will be useful, with no restrictions except giving credit where it is due.
No GPLs, Berkeley copyrights or any of that nonsense.  The author assumes NO
responsibility for how anyone uses it.  If netcat makes you rich somehow and
you're feeling generous, mail me a check.  If you are affiliated in any way
with Microsoft Network, get a life.  Always ski in control.  Comments,
questions, and patches to hobbit@avian.org.

Building
========

Compiling is fairly straightforward.  Examine the Makefile for a SYSTYPE that
matches yours, and do "make <systype>".  The executable "nc" should appear.
If there is no relevant SYSTYPE section, try "generic".  If you create new
sections for generic.h and Makefile to support another platform, please follow
the given format and mail back the diffs.

There are a couple of other settable #defines in netcat.c, which you can
include as DFLAGS="-DTHIS -DTHAT" to your "make" invocation without having to
edit the Makefile.  See the following discussions for what they are and do.

If you want to link against the resolver library on SunOS [recommended] and
you have BIND 4.9.x, you may need to change XLIBS=-lresolv in the Makefile to
XLIBS="-lresolv -l44bsd".

Linux sys/time.h does not really support presetting of FD_SETSIZE; a harmless
warning is issued.

Some systems may warn about pointer types for signal().  No problem, though.

Exploration of features
=======================

Where to begin?  Netcat is at the same time so simple and versatile, it's like
trying to describe everything you can do with your Swiss Army knife.  This will
go over the basics; you should also read the usage examples and notes later on
which may give you even more ideas about what this sort of tool is good for.

If no command arguments are given at all, netcat asks for them, reads a line
from standard input, and breaks it up into arguments internally.  This can be
useful when driving netcat from certain types of scripts, with the side effect
of hiding your command line arguments from "ps" displays.

The host argument can be a name or IP address.  If -n is specified, netcat
will only accept numeric IP addresses and do no DNS lookups for anything.  If
-n is not given and -v is turned on, netcat will do a full forward and reverse
name and address lookup for the host, and warn you about the all-too-common
problem of mismatched names in the DNS.  This often takes a little longer for
connection setup, but is useful to know about.  There are circumstances under
which this can *save* time, such as when you want to know the name for some IP
address and also connect there.  Netcat will just tell you all about it, saving
the manual steps of looking up the hostname yourself.  Normally mismatch-
checking is case-insensitive per the DNS spec, but you can define ANAL at
compile time to make it case-sensitive -- sometimes useful for uncovering minor
errors in your own DNS files while poking around your networks.

A port argument is required for outbound connections, and can be numeric or a
name as listed in /etc/services.  If -n is specified, only numeric arguments
are valid.  Special syntax and/or more than one port argument cause different
behavior -- see details below about port-scanning.

The -v switch controls the verbosity level of messages sent to standard error.
You will probably want to run netcat most of the time with -v turned on, so you
can see info about the connections it is trying to make.  You will probably
also want to give a smallish -w argument, which limits the time spent trying to
make a connection.  I usually alias "nc" to "nc -v -w 3", which makes it
function just about the same for things I would otherwise use telnet to do.
The timeout is easily changed by a subsequent -w argument which overrides the
earlier one.  Specifying -v more than once makes diagnostic output MORE
verbose.  If -v is not specified at all, netcat silently does its work unless
some error happens, whereupon it describes the error and exits with a nonzero
status.  Refused network connections are generally NOT considered to be errors,
unless you only asked for a single TCP port and it was refused.

Note that -w also sets the network inactivity timeout.  This does not have any
effect until standard input closes, but then if nothing further arrives from
the network in the next <timeout> seconds, netcat tries to read the net once
more for good measure, and then closes and exits.  There are a lot of network
services now that accept a small amount of input and return a large amount of
output, such as Gopher and Web servers, which is the main reason netcat was
written to "block" on the network staying open rather than standard input.
Handling the timeout this way gives uniform behavior with network servers that
*don't* close by themselves until told to.

UDP connections are opened instead of TCP when -u is specified.  These aren't
really "connections" per se since UDP is a connectionless protocol, although
netcat does internally use the "connected UDP socket" mechanism that most
kernels support.  Although netcat claims that an outgoing UDP connection is
"open" immediately, no data is sent until something is read from standard
input.  Only thereafter is it possible to determine whether there really is a
UDP server on the other end, and often you just can't tell.  Most UDP protocols
use timeouts and retries to do their thing and in many cases won't bother
answering at all, so you should specify a timeout and hope for the best.  You
will get more out of UDP connections if standard input is fed from a source
of data that looks like various kinds of server requests.

To obtain a hex dump file of the data sent either way, use "-o logfile".  The
dump lines begin with "<" or ">" to respectively indicate "from the net" or
"to the net", and contain the total count per direction, and hex and ascii
representations of the traffic.  Capturing a hex dump naturally slows netcat
down a bit, so don't use it where speed is critical.

Netcat can bind to any local port, subject to privilege restrictions and ports
that are already in use.  It is also possible to use a specific local network
source address if it is that of a network interface on your machine.  [Note:
this does not work correctly on all platforms.]  Use "-p portarg" to grab a
specific local port, and "-s ip-addr" or "-s name" to have that be your source
IP address.  This is often referred to as "anchoring the socket".  Root users
can grab any unused source port including the "reserved" ones less than 1024.
Absence of -p will bind to whatever unused port the system gives you, just like
any other normal client connection, unless you use -r [see below].

Listen mode will cause netcat to wait for an inbound connection, and then the
same data transfer happens.  Thus, you can do "nc -l -p 1234 < filename" and
when someone else connects to your port 1234, the file is sent to them whether
they wanted it or not.  Listen mode is generally used along with a local port
argument -- this is required for UDP mode, while TCP mode can have the system
assign one and tell you what it is if -v is turned on.  If you specify a target
host and optional port in listen mode, netcat will accept an inbound connection
only from that host and if you specify one, only from that foreign source port.
In verbose mode you'll be informed about the inbound connection, including what
address and port it came from, and since listening on "any" applies to several
possibilities, which address it came *to* on your end.  If the system supports
IP socket options, netcat will attempt to retrieve any such options from an
inbound connection and print them out in hex.

If netcat is compiled with -DGAPING_SECURITY_HOLE, the -e argument specifies
a program to exec after making or receiving a successful connection.  In the
listening mode, this works similarly to "inetd" but only for a single instance.
Use with GREAT CARE.  This piece of the code is normally not enabled; if you
know what you're doing, have fun.  This hack also works in UDP mode.  Note that
you can only supply -e with the name of the program, but no arguments.  If you
want to launch something with an argument list, write a two-line wrapper script
or just use inetd like always.

If netcat is compiled with -DTELNET, the -t argument enables it to respond
to telnet option negotiation [always in the negative, i.e. DONT or WONT].
This allows it to connect to a telnetd and get past the initial negotiation
far enough to get a login prompt from the server.  Since this feature has
the potential to modify the data stream, it is not enabled by default.  You
have to understand why you might need this and turn on the #define yourself.

Data from the network connection is always delivered to standard output as
efficiently as possible, using large 8K reads and writes.  Standard input is
normally sent to the net the same way, but the -i switch specifies an "interval
time" which slows this down considerably.  Standard input is still read in
large batches, but netcat then tries to find where line breaks exist and sends
one line every interval time.  Note that if standard input is a terminal, data
is already read line by line, so unless you make the -i interval rather long,
what you type will go out at a fairly normal rate.  -i is really designed
for use when you want to "measure out" what is read from files or pipes.

Port-scanning is a popular method for exploring what's out there.  Netcat
accepts its commands with options first, then the target host, and everything
thereafter is interpreted as port names or numbers, or ranges of ports in M-N
syntax.  CAVEAT: some port names in /etc/services contain hyphens -- netcat
currently will not correctly parse those, so specify ranges using numbers if
you can.  If more than one port is thus specified, netcat connects to *all* of
them, sending the same batch of data from standard input [up to 8K worth] to
each one that is successfully connected to.  Specifying multiple ports also
suppresses diagnostic messages about refused connections, unless -v is
specified twice for "more verbosity".  This way you normally get notified only
about genuinely open connections.  Example: "nc -v -w 2 -z target 20-30" will
try connecting to every port between 20 and 30 [inclusive] at the target, and
will likely inform you about an FTP server, telnet server, and mailer along the
way.  The -z switch prevents sending any data to a TCP connection and very
limited probe data to a UDP connection, and is thus useful as a fast scanning
mode just to see what ports the target is listening on.  To limit scanning
speed if desired, -i will insert a delay between each port probe.  There are
some pitfalls with regard to UDP scanning, described later, but in general it
works well.

For each range of ports specified, scanning is normally done downward within
that range.  If the -r switch is used, scanning hops randomly around within
that range and reports open ports as it finds them.  [If you want them listed
in order regardless, pipe standard error through "sort"...]  In addition, if
random mode is in effect, the local source ports are also randomized.  This
prevents netcat from exhibiting any kind of regular pattern in its scanning.
You can exert fairly fine control over your scan by judicious use of -r and
selected port ranges to cover.  If you use -r for a single connection, the
source port will have a random value above 8192, rather than the next one the
kernel would have assigned you.  Note that selecting a specific local port
with -p overrides any local-port randomization.

Many people are interested in testing network connectivity using IP source
routing, even if it's only to make sure their own firewalls are blocking
source-routed packets.  On systems that support it, the -g switch can be used
multiple times [up to 8] to construct a loose-source-routed path for your
connection, and the -G argument positions the "hop pointer" within the list.
If your network allows source-routed traffic in and out, you can test
connectivity to your own services via remote points in the internet.  Note that
although newer BSD-flavor telnets also have source-routing capability, it isn't
clearly documented and the command syntax is somewhat clumsy.  Netcat's
handling of "-g" is modeled after "traceroute".

Netcat tries its best to behave just like "cat".  It currently does nothing to
terminal input modes, and does no end-of-line conversion.  Standard input from
a terminal is read line by line with normal editing characters in effect.  You
can freely suspend out of an interactive connection and resume.  ^C or whatever
your interrupt character is will make netcat close the network connection and
exit.  A switch to place the terminal in raw mode has been considered, but so
far has not been necessary.  You can send raw binary data by reading it out of
a file or piping from another program, so more meaningful effort would be spent
writing an appropriate front-end driver.

Netcat is not an "arbitrary packet generator", but the ability to talk to raw
sockets and/or nit/bpf/dlpi may appear at some point.  Such things are clearly
useful; I refer you to Darren Reed's excellent ip_filter package, which now
includes a tool to construct and send raw packets with any contents you want.

Example uses -- the light side
==============================

Again, this is a very partial list of possibilities, but it may get you to
think up more applications for netcat.  Driving netcat with simple shell or
expect scripts is an easy and flexible way to do fairly complex tasks,
especially if you're not into coding network tools in C.  My coding isn't
particularly strong either [although undoubtedly better after writing this
thing!], so I tend to construct bare-metal tools like this that I can trivially
plug into other applications.  Netcat doubles as a teaching tool -- one can
learn a great deal about more complex network protocols by trying to simulate
them through raw connections!

An example of netcat as a backend for something else is the shell-script
Web browser, which simply asks for the relevant parts of a URL and pipes
"GET /what/ever" into a netcat connection to the server.  I used to do this
with telnet, and had to use calculated sleep times and other stupidity to
kludge around telnet's limitations.  Netcat guarantees that I get the whole
page, and since it transfers all the data unmodified, I can even pull down
binary image files and display them elsewhere later.  Some folks may find the
idea of a shell-script web browser silly and strange, but it starts up and
gets me my info a hell of a lot faster than a GUI browser and doesn't hide
any contents of links and forms and such.  This is included, as scripts/web,
along with several other web-related examples.

Netcat is an obvious replacement for telnet as a tool for talking to daemons.
For example, it is easier to type "nc host 25", talk to someone's mailer, and
just ^C out than having to type ^]c or QUIT as telnet would require you to do.
You can quickly catalog the services on your network by telling netcat to
connect to well-known services and collect greetings, or at least scan for open
ports.  You'll probably want to collect netcat's diagnostic messages in your
output files, so be sure to include standard error in the output using
`>& file' in *csh or `> file 2>&1' in bourne shell.

A scanning example: "echo QUIT | nc -v -w 5 target 20-250 500-600 5990-7000"
will inform you about a target's various well-known TCP servers, including
r-services, X, IRC, and maybe a few you didn't expect.  Sending in QUIT and
using the timeout will almost guarantee that you see some kind of greeting or
error from each service, which usually indicates what it is and what version.
[Beware of the "chargen" port, though...]  SATAN uses exactly this technique to
collect host information, and indeed some of the ideas herein were taken from
the SATAN backend tools.  If you script this up to try every host in your
subnet space and just let it run, you will not only see all the services,
you'll find out about hosts that aren't correctly listed in your DNS.  Then you
can compare new snapshots against old snapshots to see changes.  For going
after particular services, a more intrusive example is in scripts/probe.

Netcat can be used as a simple data transfer agent, and it doesn't really
matter which end is the listener and which end is the client -- input at one
side arrives at the other side as output.  It is helpful to start the listener
at the receiving side with no timeout specified, and then give the sending side
a small timeout.  That way the listener stays listening until you contact it,
and after data stops flowing the client will time out, shut down, and take the
listener with it.  Unless the intervening network is fraught with problems,
this should be completely reliable, and you can always increase the timeout.  A
typical example of something "rsh" is often used for: on one side,

	nc -l -p 1234 | uncompress -c | tar xvfp -

and then on the other side

	tar cfp - /some/dir | compress -c | nc -w 3 othermachine 1234

will transfer the contents of a directory from one machine to another, without
having to worry about .rhosts files, user accounts, or inetd configurations
at either end.  Again, it matters not which is the listener or receiver; the
"tarring" machine could just as easily be running the listener instead.  One
could conceivably use a scheme like this for backups, by having cron-jobs fire
up listeners and backup handlers [which can be restricted to specific addresses
and ports between each other] and pipe "dump" or "tar" on one machine to "dd
of=/dev/tapedrive" on another as usual.  Since netcat returns a nonzero exit
status for a denied listener connection, scripts to handle such tasks could
easily log and reject connect attempts from third parties, and then retry.

Another simple data-transfer example: shipping things to a PC that doesn't have
any network applications yet except a TCP stack and a web browser.  Point the
browser at an arbitrary port on a Unix server by telling it to download
something like http://unixbox:4444/foo, and have a listener on the Unix side
ready to ship out a file when the connect comes in.  The browser may pervert
binary data when told to save the URL, but you can dig the raw data out of
the on-disk cache.

If you build netcat with GAPING_SECURITY_HOLE defined, you can use it as an
"inetd" substitute to test experimental network servers that would otherwise
run under "inetd".  A script or program will have its input and output hooked
to the network the same way, perhaps sans some fancier signal handling.  Given
that most network services do not bind to a particular local address, whether
they are under "inetd" or not, it is possible for netcat avoid the "address
already in use" error by binding to a specific address.  This lets you [as
root, for low ports] place netcat "in the way" of a standard service, since
inbound connections are generally sent to such specifically-bound listeners
first and fall back to the ones bound to "any".  This allows for a one-off
experimental simulation of some service, without having to screw around with
inetd.conf.  Running with -v turned on and collecting a connection log from
standard error is recommended.

Netcat as well can make an outbound connection and then run a program or script
on the originating end, with input and output connected to the same network
port.  This "inverse inetd" capability could enhance the backup-server concept
described above or help facilitate things such as a "network dialback" concept.
The possibilities are many and varied here; if such things are intended as
security mechanisms, it may be best to modify netcat specifically for the
purpose instead of wrapping such functions in scripts.

Speaking of inetd, netcat will function perfectly well *under* inetd as a TCP
connection redirector for inbound services, like a "plug-gw" without the
authentication step.  This is very useful for doing stuff like redirecting
traffic through your firewall out to other places like web servers and mail
hubs, while posing no risk to the firewall machine itself.  Put netcat behind
inetd and tcp_wrappers, perhaps thusly:

	www stream tcp nowait nobody /etc/tcpd /bin/nc -w 3 realwww 80

and you have a simple and effective "application relay" with access control
and logging.  Note use of the wait time as a "safety" in case realwww isn't
reachable or the calling user aborts the connection -- otherwise the relay may
hang there forever.

You can use netcat to generate huge amounts of useless network data for
various performance testing.  For example, doing

	yes AAAAAAAAAAAAAAAAAAAAAA | nc -v -v -l -p 2222 > /dev/null

on one side and then hitting it with

	yes BBBBBBBBBBBBBBBBBBBBBB | nc othermachine 2222 > /dev/null

from another host will saturate your wires with A's and B's.  The "very
verbose" switch usage will tell you how many of each were sent and received
after you interrupt either side.  Using UDP mode produces tremendously MORE
trash per unit time in the form of fragmented 8 Kbyte mobygrams -- enough to
stress-test kernels and network interfaces.  Firing random binary data into
various network servers may help expose bugs in their input handling, which
nowadays is a popular thing to explore.  A simple example data-generator is
given in data/data.c included in this package, along with a small collection
of canned input files to generate various packet contents.  This program is
documented in its beginning comments, but of interest here is using "%r" to
generate random bytes at well-chosen points in a data stream.  If you can
crash your daemon, you likely have a security problem.

The hex dump feature may be useful for debugging odd network protocols,
especially if you don't have any network monitoring equipment handy or aren't
root where you'd need to run "tcpdump" or something.  Bind a listening netcat
to a local port, and have it run a script which in turn runs another netcat
to the real service and captures the hex dump to a log file.  This sets up a
transparent relay between your local port and wherever the real service is.
Be sure that the script-run netcat does *not* use -v, or the extra info it
sends to standard error may confuse the protocol.  Note also that you cannot
have the "listen/exec" netcat do the data capture, since once the connection
arrives it is no longer netcat that is running.

Binding to an arbitrary local port allows you to simulate things like r-service
clients, if you are root locally.  For example, feeding "^@root^@joe^@pwd^@"
[where ^@ is a null, and root/joe could be any other local/remote username
pair] into a "rsh" or "rlogin" server, FROM your port 1023 for example,
duplicates what the server expects to receive.  Thus, you can test for insecure
.rhosts files around your network without having to create new user accounts on
your client machine.  The program data/rservice.c can aid this process by
constructing the "rcmd" protocol bytes.  Doing this also prevents "rshd" from
trying to create that separate standard-error socket and still gives you an
input path, as opposed to the usual action of "rsh -n".  Using netcat for
things like this can be really useful sometimes, because rsh and rlogin
generally want a host *name* as an argument and won't accept IP addresses.  If
your client-end DNS is hosed, as may be true when you're trying to extract
backup sets on to a dumb client, "netcat -n" wins where normal rsh/rlogin is
useless.

If you are unsure that a remote syslogger is working, test it with netcat.
Make a UDP connection to port 514 and type in "<0>message", which should
correspond to "kern.emerg" and cause syslogd to scream into every file it has
open [and possibly all over users' terminals].  You can tame this down by
using a different number and use netcat inside routine scripts to send syslog
messages to places that aren't configured in syslog.conf.  For example,
"echo '<38>message' | nc -w 1 -u loggerhost 514" should send to auth.notice
on loggerhost.  The exact number may vary; check against your syslog.h first.

Netcat provides several ways for you to test your own packet filters.  If you
bind to a port normally protected against outside access and make a connection
to somewhere outside your own network, the return traffic will be coming to
your chosen port from the "outside" and should be blocked.  TCP may get through
if your filter passes all "ack syn", but it shouldn't be even doing that to low
ports on your network.  Remember to test with UDP traffic as well!  If your
filter passes at least outbound source-routed IP packets, bouncing a connection
back to yourself via some gateway outside your network will create "incoming"
traffic with your source address, which should get dropped by a correctly
configured anti-spoofing filter.  This is a "non-test" if you're also dropping
source-routing, but it's good to be able to test for that too.  Any packet
filter worth its salt will be blocking source-routed packets in both
directions, but you never know what interesting quirks you might turn up by
playing around with source ports and addresses and watching the wires with a
network monitor.

You can use netcat to protect your own workstation's X server against outside
access.  X is stupid enough to listen for connections on "any" and never tell
you when new connections arrive, which is one reason it is so vulnerable.  Once
you have all your various X windows up and running you can use netcat to bind
just to your ethernet address and listen to port 6000.  Any new connections
from outside the machine will hit netcat instead your X server, and you get a
log of who's trying.  You can either tell netcat to drop the connection, or
perhaps run another copy of itself to relay to your actual X server on
"localhost".  This may not work for dedicated X terminals, but it may be
possible to authorize your X terminal only for its boot server, and run a relay
netcat over on the server that will in turn talk to your X terminal.  Since
netcat only handles one listening connection per run, make sure that whatever
way you rig it causes another one to run and listen on 6000 soon afterward, or
your real X server will be reachable once again.  A very minimal script just
to protect yourself could be

	while true ; do
	  nc -v -l -s <your-addr> -p 6000 localhost 2
	done

which causes netcat to accept and then close any inbound connection to your
workstation's normal ethernet address, and another copy is immediately run by
the script.  Send standard error to a file for a log of connection attempts.
If your system can't do the "specific bind" thing all is not lost; run your
X server on display ":1" or port 6001, and netcat can still function as a probe
alarm by listening on 6000.

Does your shell-account provider allow personal Web pages, but not CGI scripts?
You can have netcat listen on a particular port to execute a program or script
of your choosing, and then just point to the port with a URL in your homepage.
The listener could even exist on a completely different machine, avoiding the
potential ire of the homepage-host administrators.  Since the script will get
the raw browser query as input it won't look like a typical CGI script, and
since it's running under your UID you need to write it carefully.  You may want
to write a netcat-based script as a wrapper that reads a query and sets up
environment variables for a regular CGI script.  The possibilities for using
netcat and scripts to handle Web stuff are almost endless.  Again, see the
examples under scripts/.

Example uses -- the dark side
=============================

Equal time is deserved here, since a versatile tool like this can be useful
to any Shade of Hat.  I could use my Victorinox to either fix your car or
disassemble it, right?  You can clearly use something like netcat to attack
or defend -- I don't try to govern anyone's social outlook, I just build tools.
Regardless of your intentions, you should still be aware of these threats to
your own systems.

The first obvious thing is scanning someone *else's* network for vulnerable
services.  Files containing preconstructed data, be it exploratory or
exploitive, can be fed in as standard input, including command-line arguments
to netcat itself to keep "ps" ignorant of your doings.  The more random the
scanning, the less likelihood of detection by humans, scan-detectors, or
dynamic filtering, and with -i you'll wait longer but avoid loading down the
target's network.  Some examples for crafting various standard UDP probes are
given in data/*.d.

Some configurations of packet filters attempt to solve the FTP-data problem by
just allowing such connections from the outside.  These come FROM port 20, TO
high TCP ports inside -- if you locally bind to port 20, you may find yourself
able to bypass filtering in some cases.  Maybe not to low ports "inside", but
perhaps to TCP NFS servers, X servers, Prospero, ciscos that listen on 200x
and 400x...  Similar bypassing may be possible for UDP [and maybe TCP too] if a
connection comes from port 53; a filter may assume it's a nameserver response.

Using -e in conjunction with binding to a specific address can enable "server
takeover" by getting in ahead of the real ones, whereupon you can snarf data
sent in and feed your own back out.  At the very least you can log a hex dump
of someone else's session.  If you are root, you can certainly use -s and -e to
run various hacked daemons without having to touch inetd.conf or the real
daemons themselves.  You may not always have the root access to deal with low
ports, but what if you are on a machine that also happens to be an NFS server?
You might be able to collect some interesting things from port 2049, including
local file handles.  There are several other servers that run on high ports
that are likely candidates for takeover, including many of the RPC services on
some platforms [yppasswdd, anyone?].  Kerberos tickets, X cookies, and IRC
traffic also come to mind.  RADIUS-based terminal servers connect incoming
users to shell-account machines on a high port, usually 1642 or thereabouts.
SOCKS servers run on 1080.  Do "netstat -a" and get creative.

There are some daemons that are well-written enough to bind separately to all
the local interfaces, possibly with an eye toward heading off this sort of
problem.  Named from recent BIND releases, and NTP, are two that come to mind.
Netstat will show these listening on address.53 instead of *.53.  You won't
be able to get in front of these on any of the real interface addresses, which
of course is especially interesting in the case of named, but these servers
sometimes forget about things like "alias" interface addresses or interfaces
that appear later on such as dynamic PPP links.  There are some hacked web
servers and versions of "inetd" floating around that specifically bind as well,
based on a configuration file -- these generally *are* bound to alias addresses
to offer several different address-based services from one machine.

Using -e to start a remote backdoor shell is another obvious sort of thing,
easier than constructing a file for inetd to listen on "ingreslock" or
something, and you can access-control it against other people by specifying a
client host and port.  Experience with this truly demonstrates how fragile the
barrier between being "logged in" or not really is, and is further expressed by
scripts/bsh.  If you're already behind a firewall, it may be easier to make an
*outbound* connection and then run a shell; a small wrapper script can
periodically try connecting to a known place and port, you can later listen
there until the inbound connection arrives, and there's your shell.  Running
a shell via UDP has several interesting features, although be aware that once
"connected", the UDP stub sockets tend to show up in "netstat" just like TCP
connections and may not be quite as subtle as you wanted.  Packets may also be
lost, so use TCP if you need reliable connections.  But since UDP is
connectionless, a hookup of this sort will stick around almost forever, even if
you ^C out of netcat or do a reboot on your side, and you only need to remember
the ports you used on both ends to reestablish.  And outbound UDP-plus-exec
connection creates the connected socket and starts the program immediately.  On
a listening UDP connection, the socket is created once a first packet is
received.  In either case, though, such a "connection" has the interesting side
effect that only your client-side IP address and [chosen?] source port will
thereafter be able to talk to it.  Instant access control!  A non-local third
party would have to do ALL of the following to take over such a session:

	forge UDP with your source address [trivial to do; see below]
	guess the port numbers of BOTH ends, or sniff the wire for them
	arrange to block ICMP or UDP return traffic between it and your real
	  source, so the session doesn't die with a network write error.

The companion program data/rservice.c is helpful in scripting up any sort of
r-service username or password guessing attack.  The arguments to "rservice"
are simply the strings that get null-terminated and passed over an "rcmd"-style
connection, with the assumption that the client does not need a separate
standard-error port.  Brute-force password banging is best done via "rexec" if
it is available since it is less likely to log failed attempts.  Thus, doing
"rservice joe joespass pwd | nc target exec" should return joe's home dir if
the password is right, or "Permission denied."  Plug in a dictionary and go to
town.  If you're attacking rsh/rlogin, remember to be root and bind to a port
between 512 and 1023 on your end, and pipe in "rservice joe joe pwd" and such.

Netcat can prevent inadvertently sending extra information over a telnet
connection.  Use "nc -t" in place of telnet, and daemons that try to ask for
things like USER and TERM environment variables will get no useful answers, as
they otherwise would from a more recent telnet program.  Some telnetds actually
try to collect this stuff and then plug the USER variable into "login" so that
the caller is then just asked for a password!  This mechanism could cause a
login attempt as YOUR real username to be logged over there if you use a
Borman-based telnet instead of "nc -t".

Got an unused network interface configured in your kernel [e.g. SLIP], or
support for alias addresses?  Ifconfig one to be any address you like, and bind
to it with -s to enable all sorts of shenanigans with bogus source addresses.
The interface probably has to be UP before this works; some SLIP versions
need a far-end address before this is true.  Hammering on UDP services is then
a no-brainer.  What you can do to an unfiltered syslog daemon should be fairly
obvious; trimming the conf file can help protect against it.  Many routers out
there still blindly believe what they receive via RIP and other routing
protocols.  Although most UDP echo and chargen servers check if an incoming
packet was sent from *another* "internal" UDP server, there are many that still
do not, any two of which [or many, for that matter] could keep each other
entertained for hours at the expense of bandwidth.  And you can always make
someone wonder why she's being probed by nsa.gov.

Your TCP spoofing possibilities are mostly limited to destinations you can
source-route to while locally bound to your phony address.  Many sites block
source-routed packets these days for precisely this reason.  If your kernel
does oddball things when sending source-routed packets, try moving the pointer
around with -G.  You may also have to fiddle with the routing on your own
machine before you start receiving packets back.  Warning: some machines still
send out traffic using the source address of the outbound interface, regardless
of your binding, especially in the case of localhost.  Check first.  If you can
open a connection but then get no data back from it, the target host is
probably killing the IP options on its end [this is an option inside TCP
wrappers and several other packages], which happens after the 3-way handshake
is completed.  If you send some data and observe the "send-q" side of "netstat"
for that connection increasing but never getting sent, that's another symptom.
Beware: if Sendmail 8.7.x detects a source-routed SMTP connection, it extracts
the hop list and sticks it in the Received: header!

SYN bombing [sometimes called "hosing"] can disable many TCP servers, and if
you hit one often enough, you can keep it unreachable for days.  As is true of
many other denial-of-service attacks, there is currently no defense against it
except maybe at the human level.  Making kernel SOMAXCONN considerably larger
than the default and the half-open timeout smaller can help, and indeed some
people running large high-performance web servers have *had* to do that just to
handle normal traffic.  Taking out mailers and web servers is sociopathic, but
on the other hand it is sometimes useful to be able to, say, disable a site's
identd daemon for a few minutes.  If someone realizes what is going on,
backtracing will still be difficult since the packets have a phony source
address, but calls to enough ISP NOCs might eventually pinpoint the source.
It is also trivial for a clueful ISP to watch for or even block outgoing
packets with obviously fake source addresses, but as we know many of them are
not clueful or willing to get involved in such hassles.  Besides, outbound
packets with an [otherwise unreachable] source address in one of their net
blocks would look fairly legitimate.

Notes
=====

A discussion of various caveats, subtleties, and the design of the innards.

As of version 1.07 you can construct a single file containing command arguments
and then some data to transfer.  Netcat is now smart enough to pick out the
first line and build the argument list, and send any remaining data across the
net to one or multiple ports.  The first release of netcat had trouble with
this -- it called fgets() for the command line argument, which behind the
scenes does a large read() from standard input, perhaps 4096 bytes or so, and
feeds that out to the fgets() library routine.  By the time netcat 1.00 started
directly read()ing stdin for more data, 4096 bytes of it were gone.  It now
uses raw read() everywhere and does the right thing whether reading from files,
pipes, or ttys.  If you use this for multiple-port connections, the single
block of data will now be a maximum of 8K minus the first line.  Improvements
have been made to the logic in sending the saved chunk to each new port.  Note
that any command-line arguments hidden using this mechanism could still be
extracted from a core dump.

When netcat receives an inbound UDP connection, it creates a "connected socket"
back to the source of the connection so that it can also send out data using
normal write().  Using this mechanism instead of recvfrom/sendto has several
advantages -- the read/write select loop is simplified, and ICMP errors can in
effect be received by non-root users.  However, it has the subtle side effect
that if further UDP packets arrive from the caller but from different source
ports, the listener will not receive them.  UDP listen mode on a multihomed
machine may have similar quirks unless you specifically bind to one of its
addresses.  It is not clear that kernel support for UDP connected sockets
and/or my understanding of it is entirely complete here, so experiment...

You should be aware of some subtleties concerning UDP scanning.  If -z is on,
netcat attempts to send a single null byte to the target port, twice, with a
small time in between.  You can either use the -w timeout, or netcat will try
to make a "sideline" TCP connection to the target to introduce a small time
delay equal to the round-trip time between you and the target.  Note that if
you have a -w timeout and -i timeout set, BOTH take effect and you wait twice
as long.  The TCP connection is to a normally refused port to minimize traffic,
but if you notice a UDP fast-scan taking somewhat longer than it should, it
could be that the target is actually listening on the TCP port.  Either way,
any ICMP port-unreachable messages from the target should have arrived in the
meantime.  The second single-byte UDP probe is then sent.  Under BSD kernels,
the ICMP error is delivered to the "connected socket" and the second write
returns an error, which tells netcat that there is NOT a UDP service there.
While Linux seems to be a fortunate exception, under many SYSV derived kernels
the ICMP is not delivered, and netcat starts reporting that *all* the ports are
"open" -- clearly wrong.  [Some systems may not even *have* the "udp connected
socket" concept, and netcat in its current form will not work for UDP at all.]
If -z is specified and only one UDP port is probed, netcat's exit status
reflects whether the connection was "open" or "refused" as with TCP.

It may also be that UDP packets are being blocked by filters with no ICMP error
returns, in which case everything will time out and return "open".  This all
sounds backwards, but that's how UDP works.  If you're not sure, try "echo
w00gumz | nc -u -w 2 target 7" to see if you can reach its UDP echo port at
all.  You should have no trouble using a BSD-flavor system to scan for UDP
around your own network, although flooding a target with the high activity that
-z generates will cause it to occasionally drop packets and indicate false
"opens".  A more "correct" way to do this is collect and analyze the ICMP
errors, as does SATAN's "udp_scan" backend, but then again there's no guarantee
that the ICMP gets back to you either.  Udp_scan also does the zero-byte
probes but is excruciatingly careful to calculate its own round-trip timing
average and dynamically set its own response timeouts along with decoding any
ICMP received.  Netcat uses a much sleazier method which is nonetheless quite
effective.  Cisco routers are known to have a "dead time" in between ICMP
responses about unreachable UDP ports, so a fast scan of a cisco will show
almost everything "open".  If you are looking for a specific UDP service, you
can construct a file containing the right bytes to trigger a response from the
other end and send that as standard input.  Netcat will read up to 8K of the
file and send the same data to every UDP port given.  Note that you must use a
timeout in this case [as would any other UDP client application] since the
two-write probe only happens if -z is specified.

Many telnet servers insist on a specific set of option negotiations before
presenting a login banner.  On a raw connection you will see this as small
amount of binary gook.  My attempts to create fixed input bytes to make a
telnetd happy worked some places but failed against newer BSD-flavor ones,
possibly due to timing problems, but there are a couple of much better
workarounds.  First, compile with -DTELNET and use -t if you just want to get
past the option negotiation and talk to something on a telnet port.  You will
still see the binary gook -- in fact you'll see a lot more of it as the options
are responded to behind the scenes.  The telnet responder does NOT update the
total byte count, or show up in the hex dump -- it just responds negatively to
any options read from the incoming data stream.  If you want to use a normal
full-blown telnet to get to something but also want some of netcat's features
involved like settable ports or timeouts, construct a tiny "foo" script:

	#! /bin/sh
	exec nc -otheroptions targethost 23

and then do

	nc -l -p someport -e foo localhost &
	telnet localhost someport

and your telnet should connect transparently through the exec'ed netcat to
the target, using whatever options you supplied in the "foo" script.  Don't
use -t inside the script, or you'll wind up sending *two* option responses.

I've observed inconsistent behavior under some Linuxes [perhaps just older
ones?] when binding in listen mode.  Sometimes netcat binds only to "localhost"
if invoked with no address or port arguments, and sometimes it is unable to
bind to a specific address for listening if something else is already listening
on "any".  The former problem can be worked around by specifying "-s 0.0.0.0",
which will do the right thing despite netcat claiming that it's listening on
[127.0.0.1].  This is a known problem -- for example, there's a mention of it
in the makefile for SOCKS.  On the flip side, binding to localhost and sending
packets to some other machine doesn't work as you'd expect -- they go out with
the source address of the sending interface instead.  The Linux kernel contains
a specific check to ensure that packets from 127.0.0.1 are never sent to the
wire; other kernels may contain similar code.  Linux, of course, *still*
doesn't support source-routing, but they claim that it and many other network
improvements are at least breathing hard.

There are several possible errors associated with making TCP connections, but
to specifically see anything other than "refused", one must wait the full
kernel-defined timeout for a connection to fail.  Netcat's mechanism of
wrapping an alarm timer around the connect prevents the *real* network error
from being returned -- "errno" at that point indicates "interrupted system
call" since the connect attempt was interrupted.  Some old 4.3 BSD kernels
would actually return things like "host unreachable" immediately if that was
the case, but most newer kernels seem to wait the full timeout and *then* pass
back the real error.  Go figure.  In this case, I'd argue that the old way was
better, despite those same kernels generally being the ones that tear down
*established* TCP connections when ICMP-bombed.

Incoming socket options are passed to applications by the kernel in the
kernel's own internal format.  The socket-options structure for source-routing
contains the "first-hop" IP address first, followed by the rest of the real
options list.  The kernel uses this as is when sending reply packets -- the
structure is therefore designed to be more useful to the kernel than to humans,
but the hex dump of it that netcat produces is still useful to have.

Kernels treat source-routing options somewhat oddly, but it sort of makes sense
once one understands what's going on internally.  The options list of addresses
must contain hop1, hop2, ..., destination.  When a source-routed packet is sent
by the kernel [at least BSD], the actual destination address becomes irrelevant
because it is replaced with "hop1", "hop1" is removed from the options list,
and all the other addresses in the list are shifted up to fill the hole.  Thus
the outbound packet is sent from your chosen source address to the first
*gateway*, and the options list now contains hop2, ..., destination.  During
all this address shuffling, the kernel does NOT change the pointer value, which
is why it is useful to be able to set the pointer yourself -- you can construct
some really bizarre return paths, and send your traffic fairly directly to the
target but around some larger loop on the way back.  Some Sun kernels seem to
never flip the source-route around if it contains less than three hops, never
reset the pointer anyway, and tries to send the packet [with options containing
a "completed" source route!!] directly back to the source.  This is way broken,
of course.  [Maybe ipforwarding has to be on?  I haven't had an opportunity to
beat on it thoroughly yet.]

"Credits" section: The original idea for netcat fell out of a long-standing
desire and fruitless search for a tool resembling it and having the same
features.  After reading some other network code and realizing just how many
cool things about sockets could be controlled by the calling user, I started
on the basics and the rest fell together pretty quickly.  Some port-scanning
ideas were taken from Venema/Farmer's SATAN tool kit, and Pluvius' "pscan"
utility.  Healthy amounts of BSD kernel source were perused in an attempt to
dope out socket options and source-route handling; additional help was obtained
from Dave Borman's telnet sources.  The select loop is loosely based on fairly
well-known code from "rsh" and Richard Stevens' "sock" program [which itself is
sort of a "netcat" with more obscure features], with some more paranoid
sanity-checking thrown in to guard against the distinct likelihood that there
are subtleties about such things I still don't understand.  I found the
argument-hiding method cleanly implemented in Barrett's "deslogin"; reading the
line as input allows greater versatility and is much less prone to cause
bizarre problems than the more common trick of overwriting the argv array.
After the first release, several people contributed portability fixes; they are
credited in generic.h and the Makefile.  Lauren Burka inspired the ascii art
for this revised document.  Dean Gaudet at Wired supplied a precursor to
the hex-dump code, and mudge@l0pht.com originally experimented with and
supplied code for the telnet-options responder.  Outbound "-e <prog>" resulted
from a need to quietly bypass a firewall installation.  Other suggestions and
patches have rolled in for which I am always grateful, but there are only 26
hours per day and a discussion of feature creep near the end of this document.

Netcat was written with the Russian railroad in mind -- conservatively built
and solid, but it *will* get you there.  While the coding style is fairly
"tight", I have attempted to present it cleanly [keeping *my* lines under 80
characters, dammit] and put in plenty of comments as to why certain things
are done.  Items I know to be questionable are clearly marked with "XXX".
Source code was made to be modified, but determining where to start is
difficult with some of the tangles of spaghetti code that are out there.
Here are some of the major points I feel are worth mentioning about netcat's
internal design, whether or not you agree with my approach.

Except for generic.h, which changes to adapt more platforms, netcat is a single
source file.  This has the distinct advantage of only having to include headers
once and not having to re-declare all my functions in a billion different
places.  I have attempted to contain all the gross who's-got-what-.h-file
things in one small dumping ground.  Functions are placed "dependencies-first",
such that when the compiler runs into the calls later, it already knows the
type and arguments and won't complain.  No function prototyping -- not even the
__P(()) crock -- is used, since it is more portable and a file of this size is
easy enough to check manually.  Each function has a standard-format comment
ahead of it, which is easily found using the regexp " :$".  I freely use gotos.
Loops and if-clauses are made as small and non-nested as possible, and the ends
of same *marked* for clarity [I wish everyone would do this!!].

Large structures and buffers are all malloc()ed up on the fly, slightly larger
than the size asked for and zeroed out.  This reduces the chances of damage
from those "end of the buffer" fencepost errors or runaway pointers escaping
off the end.  These things are permanent per run, so nothing needs to be freed
until the program exits.

File descriptor zero is always expected to be standard input, even if it is
closed.  If a new network descriptor winds up being zero, a different one is
asked for which will be nonzero, and fd zero is simply left kicking around
for the rest of the run.  Why?  Because everything else assumes that stdin is
always zero and "netfd" is always positive.  This may seem silly, but it was a
lot easier to code.  The new fd is obtained directly as a new socket, because
trying to simply dup() a new fd broke subsequent socket-style use of the new fd
under Solaris' stupid streams handling in the socket library.

The catch-all message and error handlers are implemented with an ample list of
phoney arguments to get around various problems with varargs.  Varargs seems
like deliberate obfuscation in the first place, and using it would also
require use of vfprintf() which not all platforms support.  The trailing
sleep in bail() is to allow output to flush, which is sometimes needed if
netcat is already on the other end of a network connection.

The reader may notice that the section that does DNS lookups seems much
gnarlier and more confusing than other parts.  This is NOT MY FAULT.  The
sockaddr and hostent abstractions are an abortion that forces the coder to
deal with it.  Then again, a lot of BSD kernel code looks like similar
struct-pointer hell.  I try to straighten it out somewhat by defining my own
HINF structure, containing names, ascii-format IP addresses, and binary IP
addresses.  I fill this structure exactly once per host argument, and squirrel
everything safely away and handy for whatever wants to reference it later.

Where many other network apps use the FIONBIO ioctl to set non-blocking I/O
on network sockets, netcat uses straightforward blocking I/O everywhere.
This makes everything very lock-step, relying on the network and filesystem
layers to feed in data when needed.  Data read in is completely written out
before any more is fetched.  This may not be quite the right thing to do under
some OSes that don't do timed select() right, but this remains to be seen.

The hexdump routine is written to be as fast as possible, which is why it does
so much work itself instead of just sprintf()ing everything together.  Each
dump line is built into a single buffer and atomically written out using the
lowest level I/O calls.  Further improvements could undoubtedly be made by
using writev() and eliminating all sprintf()s, but it seems to fly right along
as is.  If both exec-a-prog mode and a hexdump file is asked for, the hexdump
flag is deliberately turned off to avoid creating random zero-length files.
Files are opened in "truncate" mode; if you want "append" mode instead, change
the open flags in main().

main() may look a bit hairy, but that's only because it has to go down the
argv list and handle multiple ports, random mode, and exit status.  Efforts
have been made to place a minimum of code inside the getopt() loop.  Any real
work is sent off to functions in what is hopefully a straightforward way.

Obligatory vendor-bash: If "nc" had become a standard utility years ago,
the commercial vendors would have likely packaged it setuid root and with
-DGAPING_SECURITY_HOLE turned on but not documented.  It is hoped that netcat
will aid people in finding and fixing the no-brainer holes of this sort that
keep appearing, by allowing easier experimentation with the "bare metal" of
the network layer.

It could be argued that netcat already has too many features.  I have tried
to avoid "feature creep" by limiting netcat's base functionality only to those
things which are truly relevant to making network connections and the everyday
associated DNS lossage we're used to.  Option switches already have slightly
overloaded functionality.  Random port mode is sort of pushing it.  The
hex-dump feature went in later because it *is* genuinely useful.  The
telnet-responder code *almost* verges on the gratuitous, especially since it
mucks with the data stream, and is left as an optional piece.  Many people have
asked for example "how 'bout adding encryption?" and my response is that such
things should be separate entities that could pipe their data *through* netcat
instead of having their own networking code.  I am therefore not completely
enthusiastic about adding any more features to this thing, although you are
still free to send along any mods you think are useful.

Nonetheless, at this point I think of netcat as my tcp/ip swiss army knife,
and the numerous companion programs and scripts to go with it as duct tape.
Duct tape of course has a light side and a dark side and binds the universe
together, and if I wrap enough of it around what I'm trying to accomplish,
it *will* work.  Alternatively, if netcat is a large hammer, there are many
network protocols that are increasingly looking like nails by now...

_H* 960320 v1.10 RELEASE -- happy spring!
