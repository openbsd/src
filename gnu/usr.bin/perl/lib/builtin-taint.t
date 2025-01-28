#!./perl -T

BEGIN {
    chdir 't' if -d 't';
    require './test.pl';
    set_up_inc('../lib');
}

use v5.36;
no warnings 'experimental::builtin';

package FetchStoreCounter {
    sub TIESCALAR($class, @args) { bless \@args, $class }

    sub FETCH($self)    { $self->[0]->$*++ }
    sub STORE($self, $) { $self->[1]->$*++ }
}

# is_tainted
{
    use builtin qw( is_tainted );

    is(is_tainted($0), !!${^TAINT}, "\$0 is tainted (if tainting is supported)");
    ok(!is_tainted($1), "\$1 isn't tainted");

    # Invokes magic
    tie my $tied, FetchStoreCounter => (\my $fetchcount, \my $storecount);

    my $_dummy = is_tainted($tied);
    is($fetchcount, 1, 'is_tainted() invokes FETCH magic');

    $tied = is_tainted($0);
    is($storecount, 1, 'is_tainted() invokes STORE magic');

    is(prototype(\&builtin::is_tainted), '$', 'is_tainted prototype');
}

# vim: tabstop=4 shiftwidth=4 expandtab autoindent softtabstop=4

done_testing();
