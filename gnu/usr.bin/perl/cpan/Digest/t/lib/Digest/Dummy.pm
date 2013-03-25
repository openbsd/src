package Digest::Dummy;

use strict;
use vars qw($VERSION @ISA);
$VERSION = 1;

require Digest::base;
@ISA = qw(Digest::base);

sub new {
    my $class = shift;
    my $d = shift || "ooo";
    bless { d => $d }, $class;
}

sub add {}
sub digest { shift->{d} }

1;

