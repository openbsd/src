# subclass for testing customizing & subclassing

package MyPerlSource;

use strict;
use vars '@ISA';

use MyCustom;
use TAP::Parser::Source::Perl;

@ISA = qw( TAP::Parser::Source::Perl MyCustom );

sub _initialize {
    my $self = shift;
    $self->SUPER::_initialize(@_);
    $main::INIT{ ref($self) }++;
    $self->{initialized} = 1;
    return $self;
}

sub source {
    my $self = shift;
    return $self->SUPER::source(@_);
}

1;

