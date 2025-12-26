package HAS_HOOK;
use strict;
use warnings;

our $thawed_count;
our $loaded_count;

sub STORABLE_thaw {
    ++$thawed_count;
}

++$loaded_count;

1;
