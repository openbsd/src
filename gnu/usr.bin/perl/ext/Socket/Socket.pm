package Socket;

use vars qw($VERSION @ISA @EXPORT);
$VERSION = "1.5";

=head1 NAME

Socket, sockaddr_in, sockaddr_un, inet_aton, inet_ntoa - load the C socket.h defines and structure manipulators 

=head1 SYNOPSIS

    use Socket;

    $proto = getprotobyname('udp');
    socket(Socket_Handle, PF_INET, SOCK_DGRAM, $proto);
    $iaddr = gethostbyname('hishost.com');
    $port = getservbyname('time', 'udp');
    $sin = sockaddr_in($port, $iaddr);
    send(Socket_Handle, 0, 0, $sin);

    $proto = getprotobyname('tcp');
    socket(Socket_Handle, PF_INET, SOCK_STREAM, $proto);
    $port = getservbyname('smtp');
    $sin = sockaddr_in($port,inet_aton("127.1"));
    $sin = sockaddr_in(7,inet_aton("localhost"));
    $sin = sockaddr_in(7,INADDR_LOOPBACK);
    connect(Socket_Handle,$sin);

    ($port, $iaddr) = sockaddr_in(getpeername(Socket_Handle));
    $peer_host = gethostbyaddr($iaddr, AF_INET);
    $peer_addr = inet_ntoa($iaddr);

    $proto = getprotobyname('tcp');
    socket(Socket_Handle, PF_UNIX, SOCK_STREAM, $proto);
    unlink('/tmp/usock');
    $sun = sockaddr_un('/tmp/usock');
    connect(Socket_Handle,$sun);

=head1 DESCRIPTION

This module is just a translation of the C F<socket.h> file.
Unlike the old mechanism of requiring a translated F<socket.ph>
file, this uses the B<h2xs> program (see the Perl source distribution)
and your native C compiler.  This means that it has a 
far more likely chance of getting the numbers right.  This includes
all of the commonly used pound-defines like AF_INET, SOCK_STREAM, etc.

In addition, some structure manipulation functions are available:

=item inet_aton HOSTNAME

Takes a string giving the name of a host, and translates that
to the 4-byte string (structure). Takes arguments of both
the 'rtfm.mit.edu' type and '18.181.0.24'. If the host name
cannot be resolved, returns undef.

=item inet_ntoa IP_ADDRESS

Takes a four byte ip address (as returned by inet_aton())
and translates it into a string of the form 'd.d.d.d'
where the 'd's are numbers less than 256 (the normal
readable four dotted number notation for internet addresses).

=item INADDR_ANY

Note: does not return a number, but a packed string.

Returns the 4-byte wildcard ip address which specifies any
of the hosts ip addresses. (A particular machine can have
more than one ip address, each address corresponding to
a particular network interface. This wildcard address
allows you to bind to all of them simultaneously.)
Normally equivalent to inet_aton('0.0.0.0').

=item INADDR_LOOPBACK

Note - does not return a number.

Returns the 4-byte loopback address. Normally equivalent
to inet_aton('localhost').

=item INADDR_NONE

Note - does not return a number.

Returns the 4-byte invalid ip address. Normally equivalent
to inet_aton('255.255.255.255').

=item sockaddr_in PORT, ADDRESS

=item sockaddr_in SOCKADDR_IN

In an array context, unpacks its SOCKADDR_IN argument and returns an array
consisting of (PORT, ADDRESS).  In a scalar context, packs its (PORT,
ADDRESS) arguments as a SOCKADDR_IN and returns it.  If this is confusing,
use pack_sockaddr_in() and unpack_sockaddr_in() explicitly.

=item pack_sockaddr_in PORT, IP_ADDRESS

Takes two arguments, a port number and a 4 byte IP_ADDRESS (as returned by
inet_aton()). Returns the sockaddr_in structure with those arguments
packed in with AF_INET filled in.  For internet domain sockets, this
structure is normally what you need for the arguments in bind(),
connect(), and send(), and is also returned by getpeername(),
getsockname() and recv().

=item unpack_sockaddr_in SOCKADDR_IN

Takes a sockaddr_in structure (as returned by pack_sockaddr_in()) and
returns an array of two elements: the port and the 4-byte ip-address.
Will croak if the structure does not have AF_INET in the right place.

=item sockaddr_un PATHNAME

=item sockaddr_un SOCKADDR_UN

In an array context, unpacks its SOCKADDR_UN argument and returns an array
consisting of (PATHNAME).  In a scalar context, packs its PATHANE
arguments as a SOCKADDR_UN and returns it.  If this is confusing, use
pack_sockaddr_un() and unpack_sockaddr_un() explicitly.
These are only supported if your system has <sys/un.h>.

=item pack_sockaddr_un PATH

Takes one argument, a pathname. Returns the sockaddr_un structure with
that path packed in with AF_UNIX filled in. For unix domain sockets, this
structure is normally what you need for the arguments in bind(),
connect(), and send(), and is also returned by getpeername(),
getsockname() and recv().

=item unpack_sockaddr_un SOCKADDR_UN

Takes a sockaddr_un structure (as returned by pack_sockaddr_un())
and returns the pathname.  Will croak if the structure does not
have AF_UNIX in the right place.

=cut

use Carp;

require Exporter;
use AutoLoader;
require DynaLoader;
@ISA = qw(Exporter DynaLoader);
@EXPORT = qw(
	inet_aton inet_ntoa pack_sockaddr_in unpack_sockaddr_in
	pack_sockaddr_un unpack_sockaddr_un
	sockaddr_in sockaddr_un
	INADDR_ANY INADDR_LOOPBACK INADDR_NONE
	AF_802
	AF_APPLETALK
	AF_CCITT
	AF_CHAOS
	AF_DATAKIT
	AF_DECnet
	AF_DLI
	AF_ECMA
	AF_GOSIP
	AF_HYLINK
	AF_IMPLINK
	AF_INET
	AF_LAT
	AF_MAX
	AF_NBS
	AF_NIT
	AF_NS
	AF_OSI
	AF_OSINET
	AF_PUP
	AF_SNA
	AF_UNIX
	AF_UNSPEC
	AF_X25
	MSG_DONTROUTE
	MSG_MAXIOVLEN
	MSG_OOB
	MSG_PEEK
	PF_802
	PF_APPLETALK
	PF_CCITT
	PF_CHAOS
	PF_DATAKIT
	PF_DECnet
	PF_DLI
	PF_ECMA
	PF_GOSIP
	PF_HYLINK
	PF_IMPLINK
	PF_INET
	PF_LAT
	PF_MAX
	PF_NBS
	PF_NIT
	PF_NS
	PF_OSI
	PF_OSINET
	PF_PUP
	PF_SNA
	PF_UNIX
	PF_UNSPEC
	PF_X25
	SOCK_DGRAM
	SOCK_RAW
	SOCK_RDM
	SOCK_SEQPACKET
	SOCK_STREAM
	SOL_SOCKET
	SOMAXCONN
	SO_ACCEPTCONN
	SO_BROADCAST
	SO_DEBUG
	SO_DONTLINGER
	SO_DONTROUTE
	SO_ERROR
	SO_KEEPALIVE
	SO_LINGER
	SO_OOBINLINE
	SO_RCVBUF
	SO_RCVLOWAT
	SO_RCVTIMEO
	SO_REUSEADDR
	SO_SNDBUF
	SO_SNDLOWAT
	SO_SNDTIMEO
	SO_TYPE
	SO_USELOOPBACK
);

sub sockaddr_in {
    if (@_ == 6 && !wantarray) { # perl5.001m compat; use this && die
	my($af, $port, @quad) = @_;
	carp "6-ARG sockaddr_in call is deprecated" if $^W;
	pack_sockaddr_in($port, inet_aton(join('.', @quad)));
    } elsif (wantarray) {
	croak "usage:   (port,iaddr) = sockaddr_in(sin_sv)" unless @_ == 1;
        unpack_sockaddr_in(@_);
    } else {
	croak "usage:   sin_sv = sockaddr_in(port,iaddr))" unless @_ == 2;
        pack_sockaddr_in(@_);
    }
}

sub sockaddr_un {
    if (wantarray) {
	croak "usage:   (filename) = sockaddr_un(sun_sv)" unless @_ == 1;
        unpack_sockaddr_un(@_);
    } else {
	croak "usage:   sun_sv = sockaddr_un(filename)" unless @_ == 1;
        pack_sockaddr_un(@_);
    }
}


sub AUTOLOAD {
    my($constname);
    ($constname = $AUTOLOAD) =~ s/.*:://;
    my $val = constant($constname, @_ ? $_[0] : 0);
    if ($! != 0) {
	if ($! =~ /Invalid/) {
	    $AutoLoader::AUTOLOAD = $AUTOLOAD;
	    goto &AutoLoader::AUTOLOAD;
	}
	else {
	    my ($pack,$file,$line) = caller;
	    croak "Your vendor has not defined Socket macro $constname, used";
	}
    }
    eval "sub $AUTOLOAD { $val }";
    goto &$AUTOLOAD;
}

bootstrap Socket $VERSION;

# Preloaded methods go here.  Autoload methods go after __END__, and are
# processed by the autosplit program.

1;
__END__
