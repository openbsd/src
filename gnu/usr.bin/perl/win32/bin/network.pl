##
## Jeffrey Friedl (jfriedl@omron.co.jp)
## Copyri.... ah hell, just take it.
##
## July 1994
##
package network;
$version = "950311.5";

## version 950311.5 -- turned off warnings when requiring 'socket.ph';
## version 941028.4 -- some changes to quiet perl5 warnings.
## version 940826.3 -- added check for "socket.ph", and alternate use of
## socket STREAM value for SunOS5.x
##

## BLURB:
## A few simple and easy-to-use routines to make internet connections. 
## Similar to "chat2.pl" (but actually commented, and a bit more portable).
## Should work even on SunOS5.x.
##

##>
##
## connect_to() -- make an internet connection to a server.
##
## Two uses:
##	$error = &network'connect_to(*FILEHANDLE, $fromsockaddr, $tosockaddr)
##      $error = &network'connect_to(*FILEHANDLE, $hostname, $portnum)
##
## Makes the given connection and returns an error string, or undef if
## no error.
##
## In the first form, FROMSOCKADDR and TOSOCKADDR are of the form returned
## by SOCKET'GET_ADDR and SOCKET'MY_ADDR.
##
##<
sub connect_to
{
    local(*FD, $arg1, $arg2) = @_;
    local($from, $to)   = ($arg1, $arg2); ## for one interpretation.
    local($host, $port) = ($arg1, $arg2); ## for the other

    if (defined($to) && length($from)==16 && length($to)==16) {
	## ok just as is
    } elsif (defined($host)) {
	$to = &get_addr($host, $port);
	return qq/unknown address "$host"/ unless defined $to;
	$from = &my_addr;
    } else {
	return "unknown arguments to network'connect_to";
    }

    return "connect_to failed (socket: $!)"  unless &my_inet_socket(*FD);
    return "connect_to failed (bind: $!)"    unless bind(FD, $from);
    return "connect_to failed (connect: $!)" unless connect(FD, $to);
    local($old) = select(FD); $| = 1; select($old);
    undef;
}



##>
##
## listen_at() - used by a server to indicate that it will accept requests
##               at the port number given.
##
## Used as
##	$error = &network'listen_at(*LISTEN, $portnumber);
## (returns undef upon success)
##
## You can then do something like
##     $addr = accept(REMOTE, LISTEN);
##     print "contact from ", &network'addr_to_ascii($addr), ".\n";
##     while (<REMOTE>) {
##        .... process request....
##     }
##     close(REMOTE);
##
##<
sub listen_at
{
    local(*FD, $port) = @_;
    local($empty) = pack('S n a4 x8', 2 ,$port, "\0\0\0\0");
    return "listen_for failed (socket: $!)"  unless &my_inet_socket(*FD);
    return "listen_for failed (bind: $!)"    unless bind(FD, $empty);
    return "listen_for failed (listen: $!)"  unless listen(FD, 5);
    local($old) = select(FD); $| = 1; select($old);
    undef;
}


##>
##
## Given an internal packed internet address (as returned by &connect_to
## or &get_addr), return a printable ``1.2.3.4'' version.
##
##<
sub addr_to_ascii
{
    local($addr) = @_;
    return "bad arg" if length $addr != 16;
    return join('.', unpack("CCCC", (unpack('S n a4 x8', $addr))[2]));
}

##
## 
## Given a host and a port name, returns the packed socket addresss.
## Mostly for internal use.
##
##
sub get_addr
{
    local($host, $port) = @_;
    return $addr{$host,$port} if defined $addr{$host,$port};
    local($addr);

    if ($host =~ m/^\d+\.\d+\.\d+\.\d+$/)
    {
	$addr = pack("C4", split(/\./, $host));
    }
    elsif ($addr = (gethostbyname($host))[4], !defined $addr)
    {
        local(@lookup) = `nslookup $host 2>&1`;
	if (@lookup)
	{
	    local($lookup) = join('', @lookup[2 .. $#lookup]);
	    if ($lookup =~ m/^Address:\s*(\d+\.\d+\.\d+\.\d+)/) {
	        $addr = pack("C4", split(/\./, $1));
	    }
	}
	if (!defined $addr) {
	    ## warn "$host: SOL, dude\n";
	    return undef;
	}
    }
    $addr{$host,$port} = pack('S n a4 x8', 2 ,$port, $addr);
}


##
## my_addr()
## Returns the packed socket address of the local host (port 0)
## Mostly for internal use.
##
##
sub my_addr
{
	local(@x) = gethostbyname('localhost');
	local(@y) = gethostbyname($x[0]);
#	local($name,$aliases,$addrtype,$length,@addrs) = gethostbyname($x[0]);
#	local(@bytes) = unpack("C4",$addrs[0]);
#    	return pack('S n a4 x8', 2 ,0, $addr);
    	return pack('S n a4 x8', 2 ,0, $y[4]);
}


##
## my_inet_socket(*FD);
##
## Local routine to do socket(PF_INET, SOCK_STREAM, AF_NS).
## Takes care of figuring out the proper values for the args. Hopefully.
##
## Returns the same value as 'socket'.
##
sub my_inet_socket
{
    local(*FD) = @_;
    local($socket);

    if (!defined $socket_values_queried)
    {
	## try to load some "socket.ph"
	if (!defined &main'_SYS_SOCKET_H_) {
	  eval 'package main;
	        local($^W) = 0;
                require("sys/socket.ph")||require("socket.ph");';
	}

	## we'll use "the regular defaults" if for PF_INET and AF_NS if unknown
	$PF_INET     = defined &main'PF_INET ? &main'PF_INET : 2;
	$AF_NS       = defined &main'AF_NS   ? &main'AF_NS   : 6;
	$SOCK_STREAM = &main'SOCK_STREAM if defined &main'SOCK_STREAM;

	$socket_values_queried = 1;
    }

    if (defined $SOCK_STREAM) {
	$socket = socket(FD, $PF_INET, $SOCK_STREAM, $AF_NS);
    } else {
	##
	## We'll try the "regular default" of 1. If that returns a
	## "not supported" error, we'll try 2, which SunOS5.x uses.
	##
	$socket = socket(FD, $PF_INET, 1, $AF_NS);
	if ($socket) {
	    $SOCK_STREAM = 1; ## got it.
	} elsif ($! =~ m/not supported/i) {
	    ## we'll just assume from now on that it's 2.
	    $socket = socket(FD, $PF_INET, $SOCK_STREAM = 2, $AF_NS);
	}
    }
    $socket;
}

## This here just to quiet -w warnings.
sub dummy {
  1 || $version || &dummy;
}

1;
__END__
