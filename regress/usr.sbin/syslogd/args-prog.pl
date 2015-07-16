# Test with default values, that is:
# The client writes a message to Sys::Syslog native method.
# The syslogd writes it into a file and through a pipe.
# The syslogd passes it via UDP to the loghost.
# The server receives the message on its UDP socket.
# Find the message in client, file, pipe, syslogd, server log.
# Check messages in special log files selected with !prog !!prog !*.

use strict;
use warnings;
use Cwd;

my %log;
@log{qw(foo bar foobar)} = ();
foreach my $name (keys %log) {
    $log{$name} = getcwd()."/$name.log";
    open(my $fh, '>', $log{$name})
	or die "Create $log{$name} failed: $!";
}

sub check_file {
    my ($name, $pattern) = @_;
    check_pattern($name, $log{$name}, $pattern, \&filegrep);
}

our %args = (
    syslogd => {
	conf => <<"EOF",
!syslogd
*.*	$log{foo}
!!syslogd-regress
*.*	$log{bar}
!*
*.*	$log{foobar}
EOF
    },
    check => sub {
	check_file("foo",    { get_testgrep() => 0, qr/syslogd: start/ => 1 });
	check_file("bar",    { get_testgrep() => 1, qr/syslogd: start/ => 0 });
	check_file("foobar", { get_testgrep() => 0, qr/syslogd: start/ => 1 });
    },
);

1;
