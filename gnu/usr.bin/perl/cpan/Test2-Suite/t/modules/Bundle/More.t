use strict;
use warnings;
use Test2::Bundle::More;
use Test2::Tools::Exports;

imported_ok qw{
    ok pass fail skip todo diag note
    plan skip_all done_testing BAIL_OUT

    is isnt like unlike is_deeply cmp_ok isa_ok

    can_ok
    subtest
};

ok(Test2::Plugin::ExitSummary->active, "Exit Summary is loaded");

done_testing;

1;

__END__

