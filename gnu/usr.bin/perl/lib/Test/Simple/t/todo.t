#!perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = '../lib';
    }
}

BEGIN {
    require Test::Harness;
    use Test::More;

    if( $Test::Harness::VERSION < 1.23 ) {
        plan skip_all => 'Need Test::Harness 1.23 or up';
    }
    else {
        plan tests => 15;
    }
}

$Why = 'Just testing the todo interface.';

TODO: {
    local $TODO = $Why;

    fail("Expected failure");
    fail("Another expected failure");
}


pass("This is not todo");


TODO: {
    local $TODO = $Why;

    fail("Yet another failure");
}

pass("This is still not todo");


TODO: {
    local $TODO = "testing that error messages don't leak out of todo";

    ok( 'this' eq 'that',   'ok' );

    like( 'this', '/that/', 'like' );
    is(   'this', 'that',   'is' );
    isnt( 'this', 'this',   'isnt' );

    can_ok('Fooble', 'yarble');
    isa_ok('Fooble', 'yarble');
    use_ok('Fooble');
    require_ok('Fooble');
}


TODO: {
    todo_skip "Just testing todo_skip", 2;

    fail("Just testing todo");
    die "todo_skip should prevent this";
    pass("Again");
}
