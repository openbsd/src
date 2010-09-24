# subclass for testing customizing & subclassing

package MySource;

use strict;
use vars '@ISA';

use MyCustom;
use TAP::Parser::Source;

@ISA = qw( TAP::Parser::Source MyCustom );

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

sub get_stream {
    my $self   = shift;
    my $stream = $self->SUPER::get_stream(@_);

    # re-bless it:
    bless $stream, 'MyIterator';
}

1;
