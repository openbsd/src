package Carp::Heavy;

use Carp ();

our $VERSION = '1.29';

1;

# Most of the machinery of Carp used to be here.
# It has been moved in Carp.pm now, but this placeholder remains for
# the benefit of modules that like to preload Carp::Heavy directly.
# This must load Carp, because some modules rely on the historical
# behaviour of Carp::Heavy loading Carp.
