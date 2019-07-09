#	$OpenBSD: OpenBSD-Unveil.t,v 1.1 2019/07/09 20:41:54 afresh1 Exp $	#
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
	ok OpenBSD::Unveil::_unveil('/dev/random', 'r'),
	    "Unveiled /dev/random r";
	ok OpenBSD::Unveil::_unveil('/dev/null',   'wc'),
	    "Unvailed /dev/null wc";

	ok !-e '/dev/zero',   "Can't see /dev/zero";
	ok !-w '/dev/random', "Can't write to /dev/random";
	ok !-r '/dev/null',   "Can't read from /dev/null";

	ok open(my $rfh, '<', '/dev/random'), "Opened /dev/random for reading";
	ok read( $rfh, my $data, 64),         "Read from /dev/random";
	ok close($rfh),                       "Closed /dev/random";

	{
		ok open(my $wfh, '>', '/dev/null'),
		                              "Opened /dev/null for writing";
		ok print($wfh $data),         "Printed to /dev/null";
		ok close($wfh),               "Closed /dev/null";
	}

	ok OpenBSD::Unveil::_unveil('/dev/null',   'w'),
	    "Unvailed /dev/null w";
	ok OpenBSD::Unveil::_unveil(),
		"locked unveil";

	{
		ok sysopen(my $wfh, '/dev/null', O_WRONLY),
		                              "Sysopened /dev/null for writing";
		ok syswrite($wfh, $data),     "Wrote to /dev/null";
		ok close($wfh),               "Closed /dev/null";
	}

	{
		ok !open(my $wfh, '>', '/dev/null'),
			"Unable to 'open' without 'create'";
	}
};

xsunveil_ok "Invalid Path" => sub {
	chdir "/tmp" or die "Unable to chdir to /tmp";
	my $dir = File::Temp->newdir('OpenBSD-Unveil-XXXXXXXXX');
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
