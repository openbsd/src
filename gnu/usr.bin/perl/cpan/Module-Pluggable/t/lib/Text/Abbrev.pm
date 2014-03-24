package Text::Abbrev;
use strict;

sub module_pluggable {
    return bless {}, shift;
}

sub MPCHECK { "HELLO" }

1;