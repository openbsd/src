#!./perl

BEGIN {
    require($ENV{PERL_CORE} ? '../../t/test.pl' : './t/test.pl');

    use Config;
    my $reason;
    if ($ENV{PERL_CORE} and $Config{'extensions'} !~ /\bSocket\b/) {
      $reason = 'Socket was not built';
    }
    elsif ($ENV{PERL_CORE} and $Config{'extensions'} !~ /\bIO\b/) {
      $reason = 'IO was not built';
    }
    undef $reason if $^O eq 'VMS' and $Config{d_socket};
    skip_all($reason) if $reason;
}

sub compare_addr {
    no utf8;
    my $a = shift;
    my $b = shift;
    if (length($a) != length $b) {
	my $min = (length($a) < length $b) ? length($a) : length $b;
	if ($min and substr($a, 0, $min) eq substr($b, 0, $min)) {
	    printf "# Apparently: %d bytes junk at the end of %s\n# %s\n",
		abs(length($a) - length ($b)),
		$_[length($a) < length ($b) ? 1 : 0],
		"consider decreasing bufsize of recfrom.";
	    substr($a, $min) = "";
	    substr($b, $min) = "";
	}
	return 0;
    }
    my @a = unpack_sockaddr_in($a);
    my @b = unpack_sockaddr_in($b);
    "$a[0]$a[1]" eq "$b[0]$b[1]";
}

plan(7);
watchdog(15);

use Socket;
use IO::Socket qw(AF_INET SOCK_DGRAM INADDR_ANY);

$udpa = IO::Socket::INET->new(Proto => 'udp', LocalAddr => 'localhost')
     || IO::Socket::INET->new(Proto => 'udp', LocalAddr => '127.0.0.1')
    or die "$! (maybe your system does not have a localhost at all, 'localhost' or 127.0.0.1)";
ok(1);

$udpb = IO::Socket::INET->new(Proto => 'udp', LocalAddr => 'localhost')
     || IO::Socket::INET->new(Proto => 'udp', LocalAddr => '127.0.0.1')
    or die "$! (maybe your system does not have a localhost at all, 'localhost' or 127.0.0.1)";
ok(1);

$udpa->send('BORK', 0, $udpb->sockname);

ok(compare_addr($udpa->peername,$udpb->sockname, 'peername', 'sockname'));

my $where = $udpb->recv($buf="", 4);
is($buf, 'BORK');

my @xtra = ();

if (! ok(compare_addr($where,$udpa->sockname, 'recv name', 'sockname'))) {
    @xtra = (0, $udpa->sockname);
}

$udpb->send('FOObar', @xtra);
$udpa->recv($buf="", 6);
is($buf, 'FOObar');

ok(! $udpa->connected);

exit(0);

# EOF
