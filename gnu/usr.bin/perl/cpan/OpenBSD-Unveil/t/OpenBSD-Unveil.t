#	$OpenBSD: OpenBSD-Unveil.t,v 1.2 2023/07/07 02:07:35 afresh1 Exp $	#
## no critic 'version'
## no critic 'package'
# Before 'make install' is performed this script should be runnable with
# 'make test'. After 'make install' it should work as 'perl OpenBSD-Unveil.t'

#########################

use strict;
use warnings;

use Test2::IPC;
use Test::More;

use Fcntl qw< O_RDONLY O_WRONLY >;
use File::Temp;

use POSIX qw< :errno_h >;

BEGIN { use_ok('OpenBSD::Unveil') }

#########################
# UNVEIL
#########################
{
	my @calls;
	no warnings 'redefine';    ## no critic 'warnings';
	local *OpenBSD::Unveil::_unveil = sub { push @calls, \@_; return 1 };
	use warnings 'redefine';

	{
		local $@;
		eval { local $SIG{__DIE__};
		    OpenBSD::Unveil::unveil(qw< ab cx yz >) };
 		my $at = sprintf "at %s line %d.\n", __FILE__, __LINE__ - 1;
		is $@,
		    "Usage: OpenBSD::Unveil::unveil([path, permissions]) $at",
		    "Expected exception when too many params"
	}

	{
		local $@;
		eval { local $SIG{__DIE__};
		    OpenBSD::Unveil::unveil(qw< ab >) };
 		my $at = sprintf "at %s line %d.\n", __FILE__, __LINE__ - 1;
		is $@,
		    "Usage: OpenBSD::Unveil::unveil([path, permissions]) $at",
		    "Expected exception when not enough params"
	}

	ok OpenBSD::Unveil::unveil( qw< foo bar > ), "Used two args";
	ok OpenBSD::Unveil::unveil(),                "Used zero args";

	is_deeply \@calls, [ [ qw< foo bar > ], [] ],
	    "No modification to params";
}

## no critic 'private'
## no critic 'punctuation'
#########################
# _UNVEIL
#########################

sub xsunveil_ok ($$)    ## no critic 'prototypes'
{
	my ( $name, $code ) = @_;
	local $Test::Builder::Level =
	    $Test::Builder::Level + 1;    ## no critic 'package variable'

	my $pid = fork // die "Unable to fork for $name: $!\n";

	if ( !$pid ) {
		# for Test2::IPC
		OpenBSD::Unveil::_unveil('/tmp', 'rwc') || die $!;
		subtest $name, $code;
		exit 0;
	}

	waitpid $pid, 0;
	return $? >> 8;
}


xsunveil_ok "Basic Usage" => sub {
	my $tmpfile = File::Temp->new("OpenBSD-Unveil-XXXXXXXXX", TMPDIR => 1);
	$tmpfile->printflush("This is a test\n");

	ok OpenBSD::Unveil::_unveil("$tmpfile", 'r'),
	    "Unveiled tempfile r";
	ok OpenBSD::Unveil::_unveil('/dev/null',   'wc'),
	    "Unvailed /dev/null wc";

	{
		ok open(my $wfh, '>', '/dev/null'),
		                         "Opened /dev/null for writing";
		ok print($wfh "Test\n"), "Printed to /dev/null";
		ok close($wfh),          "Closed /dev/null";
	}

	ok OpenBSD::Unveil::_unveil('/dev/null',   'w'),
	    "Unvailed /dev/null w";
	ok OpenBSD::Unveil::_unveil(),
	    "locked unveil";

	ok !-e '/dev/zero', "Stat says we can't see /dev/zero";
	ok  -w $tmpfile,    "Stat says we can write to tempfile";
	ok !-r '/dev/null', "Stat says we can't read from /dev/null";

	{
		ok sysopen(my $wfh, '/dev/null', O_WRONLY),
		                              "Sysopened /dev/null for writing";
		ok syswrite($wfh, "Test\n"),  "Wrote to /dev/null";
		ok close($wfh),               "Closed /dev/null";
	}

	{
		ok open(my $rfh, '<', $tmpfile), "Opened tempfile for reading";
		ok read( $rfh, my $data, 64),    "Read from tempfile";
		ok close($rfh),                  "Closed tempfile";
	}

	{
		ok !open(my $wfh, '>', $tmpfile),
			"Unable to 'open' tempfile for writing";
		is $!, 'Permission denied', "Expected ERRNO from open";
	}

	{
		ok !open(my $wfh, '>', '/dev/null'),
			"Unable to 'open' /dev/null without 'create'";
		is $!, 'Permission denied', "Expected ERRNO from open";
	}
};

xsunveil_ok "Invalid Path" => sub {
	my $dir = File::Temp->newdir('OpenBSD-Unveil-XXXXXXXXX', TMPDIR => 1);
	ok !OpenBSD::Unveil::_unveil("$dir/nonexist/file", 'r'),
	    "Unable to unveil with incorrect permissions";
	is $!, 'No such file or directory', "Expected ERRNO from _unveil";
};

xsunveil_ok "Invalid Permissions" => sub {
	ok !OpenBSD::Unveil::_unveil('/dev/null', 'abc'),
	    "Unable to unveil with incorrect permissions";
	is $!, 'Invalid argument', "Expected ERRNO from _unveil";
};

xsunveil_ok "Try to increase permissions" => sub {
	ok OpenBSD::Unveil::_unveil('/dev/null', 'r'),
	    "Set /dev/null to r";
	TODO: { local $TODO = "Not sure why this fails";
	ok !OpenBSD::Unveil::_unveil('/dev/null', 'rwc'),
	    "Unable to increase permissions on /dev/null";
	is $!, 'Operation not permitted', "Expected ERRNO from _unveil";
	}
};

xsunveil_ok "Try to change veil after lock" => sub {
	ok OpenBSD::Unveil::_unveil(), "Locked unveil";
	ok !OpenBSD::Unveil::_unveil('/dev/null', 'r'),
	    "Unable to unveil after lock";
	is $!, 'Operation not permitted', "Expected ERRNO from _unveil";
};

#########################
done_testing;

1;    # to shut up critic
