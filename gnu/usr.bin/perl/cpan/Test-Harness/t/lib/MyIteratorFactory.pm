# subclass for testing customizing & subclassing

package MyIteratorFactory;

use strict;
use vars '@ISA';

use MyCustom;
use MyIterator;
use TAP::Parser::IteratorFactory;

@ISA = qw( TAP::Parser::IteratorFactory MyCustom );

sub make_iterator {
    my $class = shift;
    return MyIterator->new(@_);
}

1;
