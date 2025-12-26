package Term::Table::Spacer;
use strict;
use warnings;

our $VERSION = '0.024';

sub new { bless {}, $_[0] }

sub width { 1 }

sub sanitize  { }
sub mark_tail { }
sub reset     { }

1;
