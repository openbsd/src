#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

# use strict;

plan tests => 44;

# simple use cases
{
    my %h = map { $_ => uc $_ } 'a'..'z';

    is( join(':', %h{'c','d','e'}), 'c:C:d:D:e:E', "correct result and order");
    is( join(':', %h{'e','d','c'}), 'e:E:d:D:c:C', "correct result and order");
    is( join(':', %h{'e','c','d'}), 'e:E:c:C:d:D', "correct result and order");

    ok( eq_hash( { %h{'q','w'} }, { q => 'Q', w => 'W' } ), "correct hash" );

    is( join(':', %h{()}), '', "correct result for empty slice");
}

# not existing elements
{
    my %h = map { $_ => uc $_ } 'a'..'d';
    ok( eq_hash( { %h{qw(e d)} }, { e => undef, d => 'D' } ),
        "not existing returned with undef value" );

    ok( !exists $h{e}, "no autovivification" );
}

# repeated keys
{
    my %h = map { $_ => uc $_ } 'a'..'d';
    my @a = %h{ ('c') x 3 };
    ok eq_array( \@a, [ ('c', 'C') x 3 ]), "repetead keys end with repeated results";
}

# scalar context
{
    my @warn;
    local $SIG{__WARN__} = sub {push @warn, "@_"};

    my %h = map { $_ => uc $_ } 'a'..'z';
    is scalar eval"%h{'c','d','e'}", 'E', 'last element in scalar context';

    like ($warn[0],
     qr/^\%h\{\.\.\.\} in scalar context better written as \$h\{\.\.\.\}/);

    eval 'is( scalar %h{i}, "I", "correct value");';

    is (scalar @warn, 2);
    like ($warn[1],
          qr/^\%h\{"i"\} in scalar context better written as \$h\{"i"\}/);
}

# autovivification
{
    my %h = map { $_ => uc $_ } 'a'..'b';

    my @a = %h{'c','d'};
    is( join(':', map {$_//'undef'} @a), 'c:undef:d:undef', "correct result");
    ok( eq_hash( \%h, { a => 'A', b => 'B' } ), "correct hash" );
}

# hash refs
{
    my $h = { map { $_ => uc $_ } 'a'..'z' };

    is( join(':', %$h{'c','d','e'}), 'c:C:d:D:e:E', "correct result and order");
    is( join(':', %{$h}{'c','d','e'}), 'c:C:d:D:e:E', "correct result and order");
}

# no interpolation
{
    my %h = map { $_ => uc $_ } 'a'..'b';
    is( "%h{'a','b'}", q{%h{'a','b'}}, 'no interpolation within strings' );
}

# ref of a slice produces list
{
    my %h = map { $_ => uc $_ } 'a'..'z';
    my @a = \%h{ qw'c d e' };

    my $ok = 1;
    $ok = 0 if grep !ref, @a;
    ok $ok, "all elements are refs";

    is join( ':', map{ $$_ } @a ), 'c:C:d:D:e:E'
}

# lvalue usage in foreach
{
    my %h = qw(a 1 b 2 c 3);
    $_++ foreach %h{'b', 'c'};
    ok( eq_hash( \%h, { a => 1, b => 3, c => 4 } ), "correct hash" );
}

# lvalue subs in foreach
{
    my %h = qw(a 1 b 2 c 3);
    sub foo:lvalue{ %h{qw(a b)} };
    $_++ foreach foo();
    ok( eq_hash( \%h, { a => 2, b => 3, c => 3 } ), "correct hash" );
}

# errors
{
    my %h = map { $_ => uc $_ } 'a'..'b';
    # no local
    {
        local $@;
        eval 'local %h{qw(a b)}';
        like $@, qr{^Can't modify key/value hash slice in local at},
            'local dies';
    }
    # no delete
    {
        local $@;
        eval 'delete %h{qw(a b)}';
        like $@, qr{^delete argument is key/value hash slice, use hash slice},
            'delete dies';
    }
    # no assign
    {
        local $@;
        eval '%h{qw(a b)} = qw(B A)';
        like $@, qr{^Can't modify key/value hash slice in list assignment},
            'assign dies';
    }
    # lvalue subs in assignment
    {
        local $@;
        eval 'sub bar:lvalue{ %h{qw(a b)} }; bar() = "1"';
        like $@, qr{^Can't modify key/value hash slice in list assignment},
            'not allowed as result of lvalue sub';
    }
}

# warnings
{
    my @warn;
    local $SIG{__WARN__} = sub {push @warn, "@_"};

    my %h = map { $_ => uc $_ } 'a'..'c';
    {
        @warn = ();
        my $v = eval '%h{a}';
        is (scalar @warn, 1, 'warning in scalar context');
        like $warn[0],
             qr{^%h{"a"} in scalar context better written as \$h{"a"}},
            "correct warning text";
    }
    {
        @warn = ();
        my ($k,$v) = eval '%h{a}';
        is ($k, 'a');
        is ($v, 'A');
        is (scalar @warn, 0, 'no warning in list context');
    }

    # deprecated syntax
    {
        my $h = \%h;
        @warn = ();
        ok( eq_array([eval '%$h->{a}'], ['A']), 'works, but deprecated' );
        is (scalar @warn, 1, 'one warning');
        like $warn[0], qr{^Using a hash as a reference is deprecated},
            "correct warning text";

        @warn = ();
        ok( eq_array([eval '%$h->{"b","c"}'], [undef]), 'works, but deprecated' );
        is (scalar @warn, 1, 'one warning');
        like $warn[0], qr{^Using a hash as a reference is deprecated},
            "correct warning text";
    }
}

# simple case with tied
{
    require Tie::Hash;
    tie my %h, 'Tie::StdHash';
    %h = map { $_ => uc $_ } 'a'..'c';

    ok( eq_array( [%h{'b','a', 'e'}], [qw(b B a A e), undef] ),
        "works on tied" );

    ok( !exists $h{e}, "no autovivification" );
}

# keys/value/each treat argument as scalar
{
    my %h = 'a'..'b';
    my %i = (foo => \%h);
    no warnings 'syntax', 'experimental::autoderef';
    my ($k,$v) = each %i{foo=>};
    is $k, 'a', 'key returned by each %hash{key}';
    is $v, 'b', 'val returned by each %hash{key}';
    %h = 1..10;
    is join('-', sort keys %i{foo=>}), '1-3-5-7-9', 'keys %hash{key}';
    is join('-', sort values %i{foo=>}), '10-2-4-6-8', 'values %hash{key}';
}

# \% prototype expects hash deref
sub nowt_but_hash(\%) {}
eval 'nowt_but_hash %INC{bar}';
like $@, qr`^Type of arg 1 to main::nowt_but_hash must be hash \(not(?x:
           ) key/value hash slice\) at `,
    '\% prototype';
