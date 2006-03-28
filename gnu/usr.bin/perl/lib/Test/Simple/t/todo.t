#!perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = '../lib';
    }
}

use Test::More;

plan tests => 18;


$Why = 'Just testing the todo interface.';

my $is_todo;
TODO: {
    local $TODO = $Why;

    fail("Expected failure");
    fail("Another expected failure");

    $is_todo = Test::More->builder->todo;
}

pass("This is not todo");
ok( $is_todo, 'TB->todo' );


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


{
    my $warning;
    local $SIG{__WARN__} = sub { $warning = join "", @_ };
    TODO: {
        # perl gets the line number a little wrong on the first
        # statement inside a block.
        1 == 1;
#line 82
        todo_skip "Just testing todo_skip";
        fail("So very failed");
    }
    is( $warning, "todo_skip() needs to know \$how_many tests are in the ".
                  "block at $0 line 82\n",
        'todo_skip without $how_many warning' );
}
