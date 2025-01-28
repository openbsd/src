use strict;
use warnings;

# Prevent Test2::Util from making 'CAN_THREAD' a constant
my $threads;
BEGIN {
    require Test2::Util;
    local $SIG{__WARN__} = sub { 1 }; # no warnings is not sufficient on older perls
    *Test2::Util::CAN_THREAD = sub { $threads };
}

use Test2::Bundle::Extended -target => 'Test2::Require::Threads';

{
    $threads = 0;
    is($CLASS->skip(), 'This test requires a perl capable of threading.', "will skip");

    $threads = 1;
    is($CLASS->skip(), undef, "will not skip");
}

done_testing;
