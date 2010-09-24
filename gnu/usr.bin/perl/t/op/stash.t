#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = qw(../lib);
}

BEGIN { require "./test.pl"; }

plan( tests => 31 );

# Used to segfault (bug #15479)
fresh_perl_like(
    '%:: = ""',
    qr/Odd number of elements in hash assignment at - line 1\./,
    { switches => [ '-w' ] },
    'delete $::{STDERR} and print a warning',
);

# Used to segfault
fresh_perl_is(
    'BEGIN { $::{"X::"} = 2 }',
    '',
    { switches => [ '-w' ] },
    q(Insert a non-GV in a stash, under warnings 'once'),
);

{
    no warnings 'deprecated';
    ok( !defined %oedipa::maas::, q(stashes aren't defined if not used) );
    ok( !defined %{"oedipa::maas::"}, q(- work with hard refs too) );

    ok( defined %tyrone::slothrop::, q(stashes are defined if seen at compile time) );
    ok( defined %{"tyrone::slothrop::"}, q(- work with hard refs too) );

    ok( defined %bongo::shaftsbury::, q(stashes are defined if a var is seen at compile time) );
    ok( defined %{"bongo::shaftsbury::"}, q(- work with hard refs too) );
}

package tyrone::slothrop;
$bongo::shaftsbury::scalar = 1;

package main;

# Used to warn
# Unbalanced string table refcount: (1) for "A::" during global destruction.
# for ithreads.
{
    local $ENV{PERL_DESTRUCT_LEVEL} = 2;
    fresh_perl_is(
		  'package A; sub a { // }; %::=""',
		  '',
		  '',
		  );
}

# now tests in eval

ok( !eval  { no warnings 'deprecated'; defined %achtfaden:: },   'works in eval{}' );
ok( !eval q{ no warnings 'deprecated'; defined %schoenmaker:: }, 'works in eval("")' );

# now tests with strictures

{
    use strict;
    no warnings 'deprecated';
    ok( !defined %pig::, q(referencing a non-existent stash doesn't produce stricture errors) );
    ok( !exists $pig::{bodine}, q(referencing a non-existent stash element doesn't produce stricture errors) );
}

SKIP: {
    eval { require B; 1 } or skip "no B", 18;

    *b = \&B::svref_2object;
    my $CVf_ANON = B::CVf_ANON();

    my $sub = do {
        package one;
        \&{"one"};
    };
    delete $one::{one};
    my $gv = b($sub)->GV;

    isa_ok( $gv, "B::GV", "deleted stash entry leaves CV with valid GV");
    is( b($sub)->CvFLAGS & $CVf_ANON, $CVf_ANON, "...and CVf_ANON set");
    is( eval { $gv->NAME }, "__ANON__", "...and an __ANON__ name");
    is( eval { $gv->STASH->NAME }, "one", "...but leaves stash intact");

    $sub = do {
        package two;
        \&{"two"};
    };
    %two:: = ();
    $gv = b($sub)->GV;

    isa_ok( $gv, "B::GV", "cleared stash leaves CV with valid GV");
    is( b($sub)->CvFLAGS & $CVf_ANON, $CVf_ANON, "...and CVf_ANON set");
    is( eval { $gv->NAME }, "__ANON__", "...and an __ANON__ name");
    is( eval { $gv->STASH->NAME }, "__ANON__", "...and an __ANON__ stash");

    $sub = do {
        package three;
        \&{"three"};
    };
    undef %three::;
    $gv = b($sub)->GV;

    isa_ok( $gv, "B::GV", "undefed stash leaves CV with valid GV");
    is( b($sub)->CvFLAGS & $CVf_ANON, $CVf_ANON, "...and CVf_ANON set");
    is( eval { $gv->NAME }, "__ANON__", "...and an __ANON__ name");
    is( eval { $gv->STASH->NAME }, "__ANON__", "...and an __ANON__ stash");

    TODO: {
        local $TODO = "anon CVs not accounted for yet";

        my @results = split "\n", runperl(
            switches    => [ "-MB", "-l" ],
            prog        => q{
                my $sub = do {
                    package four;
                    sub { 1 };
                };
                %four:: = ();

                my $gv = B::svref_2object($sub)->GV;
                print $gv->isa(q/B::GV/) ? q/ok/ : q/not ok/;

                my $st = eval { $gv->STASH->NAME };
                print $st eq q/__ANON__/ ? q/ok/ : q/not ok/;

                my $sub = do {
                    package five;
                    sub { 1 };
                };
                undef %five::;

                $gv = B::svref_2object($sub)->GV;
                print $gv->isa(q/B::GV/) ? q/ok/ : q/not ok/;

                $st = eval { $gv->STASH->NAME };
                print $st eq q/__ANON__/ ? q/ok/ : q/not ok/;

                print q/done/;
            },
            ($^O eq 'VMS') ? (stderr => 1) : ()
        );

        ok( @results == 5 && $results[4] eq "done",
            "anon CVs in undefed stash don't segfault" )
            or todo_skip $TODO, 4;

        ok( $results[0] eq "ok", 
            "cleared stash leaves anon CV with valid GV");
        ok( $results[1] eq "ok",
            "...and an __ANON__ stash");
            
        ok( $results[2] eq "ok", 
            "undefed stash leaves anon CV with valid GV");
        ok( $results[3] eq "ok",
            "...and an __ANON__ stash");
    }
    
    # [perl #58530]
    fresh_perl_is(
        'sub foo { 1 }; use overload q/""/ => \&foo;' .
            'delete $main::{foo}; bless []',
        "",
        {},
        "no segfault with overload/deleted stash entry [#58530]",
    );
}
