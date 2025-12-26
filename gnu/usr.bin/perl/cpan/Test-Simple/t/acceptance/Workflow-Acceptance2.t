use strict;
use warnings;
use Test2::Bundle::Extended;
use Test2::Tools::Spec;

describe outer => sub {
    tests foo => sub { ok(1, 'foo') };

    describe inner => sub {
        tests bar => sub { ok(1, 'bar') };
    };
};

tests foo => sub { ok(1, 'foo') };

done_testing;
