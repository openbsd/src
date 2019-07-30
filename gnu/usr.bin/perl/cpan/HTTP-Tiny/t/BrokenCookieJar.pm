package t::BrokenCookieJar;

use strict;
use warnings;

sub new {
    my $class = shift;
    return bless {} => $class;
}

package t::BrokenCookieJar2;

use strict;
use warnings;

sub new {
    my $class = shift;
    return bless {} => $class;
}

sub add {
}

1;
