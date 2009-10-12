# subclass for testing customizing & subclassing

package MyResultFactory;

use strict;
use vars '@ISA';

use MyCustom;
use MyResult;
use TAP::Parser::ResultFactory;

@ISA = qw( TAP::Parser::ResultFactory MyCustom );

sub make_result {
    my $class = shift;

    # I know, this is not really being initialized, but
    # for consistency's sake, deal with it :)
    $main::INIT{$class}++;
    return MyResult->new(@_);
}

1;
