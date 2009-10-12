# avoid cut-n-paste exhaustion with this mixin

package MyCustom;
use strict;

sub custom {
    my $self = shift;
    $main::CUSTOM{ ref($self) }++;
    return $self;
}

1;
