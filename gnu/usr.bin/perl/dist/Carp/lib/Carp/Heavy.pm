package Carp::Heavy;

use Carp ();

our $VERSION = '1.3301';

my $cv = defined($Carp::VERSION) ? $Carp::VERSION : "undef";
if($cv ne $VERSION) {
	die "Version mismatch between Carp $cv ($INC{q(Carp.pm)}) and Carp::Heavy $VERSION ($INC{q(Carp/Heavy.pm)}).  Did you alter \@INC after Carp was loaded?\n";
}

1;

# Most of the machinery of Carp used to be here.
# It has been moved in Carp.pm now, but this placeholder remains for
# the benefit of modules that like to preload Carp::Heavy directly.
# This must load Carp, because some modules rely on the historical
# behaviour of Carp::Heavy loading Carp.
