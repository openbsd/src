#	$OpenBSD: OpenBSD-Pledge.t,v 1.3 2017/09/09 14:53:57 afresh1 Exp $	#
## no critic 'version'
## no critic 'package'
# Before 'make install' is performed this script should be runnable with
# 'make test'. After 'make install' it should work as 'perl OpenBSD-Pledge.t'

#########################

use strict;
use warnings;

use Fcntl qw( O_RDONLY O_WRONLY );
use File::Temp;

use Config;
my %sig_num;
@sig_num{ split q{ }, $Config{sig_name} } = split q{ }, $Config{sig_num};

use Test::More;
BEGIN { use_ok('OpenBSD::Pledge') }

## no critic 'private'
## no critic 'punctuation'
#########################
# PLEDGENAMES
#########################

# Here we just test that we get a small subset of names back
# because there is no point in failing if someone adds new names.

my %names = map { $_ => 1 } OpenBSD::Pledge::pledgenames();
ok $names{$_}, "$_ pledge name exists" for qw(
    stdio
    rpath
    wpath
    cpath
);

#########################
# _PLEDGE
#########################

sub xspledge_ok ($$)    ## no critic 'prototypes'
{
	my ( $name, $code ) = @_;
	local $Test::Builder::Level =
	    $Test::Builder::Level + 1;    ## no critic 'package variable'

	my $ok = 0;
	foreach my $pledge ( q{}, $name ) {
		my $dir = File::Temp->newdir('OpenBSD-Pledge-XXXXXXXXX');
		my $pid = fork // die "Unable to fork for $name: $!\n";

		if ( !$pid ) {
			chdir($dir);
			OpenBSD::Pledge::_pledge( "abort" );  # non fatal
			OpenBSD::Pledge::_pledge( "stdio $pledge" )
			    || die "[$name] $!\n";
			$code->();
			exit;
		}

		waitpid $pid, 0;

		if ($pledge) {
			$ok += is $?, 0, "[$name] OK with pledge";
		} else {
			## no critic 'numbers'
			$ok += is $? & 127, $sig_num{ABRT},
			    "[$name] ABRT without pledge";
		}
	}
	return $ok == 2;
}
xspledge_ok rpath => sub { sysopen my $fh, '/dev/random', O_RDONLY };
xspledge_ok wpath => sub { sysopen my $fh, 'FOO',         O_WRONLY };
xspledge_ok cpath => sub { mkdir q{/} };

#########################
# PLEDGE
#########################
{
	my @calls;
	no warnings 'redefine';    ## no critic 'warnings';
	local *OpenBSD::Pledge::_pledge = sub { push @calls, \@_; return 1 };
	use warnings 'redefine';

	OpenBSD::Pledge::pledge(qw( foo bar foo baz ));
	OpenBSD::Pledge::pledge( qw( foo qux baz quux ));

	is_deeply \@calls,
	    [
		[ "bar baz foo stdio" ],
		[ "baz foo quux qux stdio" ],
	    ],
	    "Sorted and unique promises, plus stdio";
}

#########################
done_testing;

1;    # to shut up critic
