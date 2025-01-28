use strict;
use warnings;

use Test2::API qw/context/;

use Test::More();
use Test2::Tools::Basic qw/todo done_testing/;

BEGIN {
    *tm_ok = \&Test::More::ok;
    *tm_pass = \&Test::More::pass;
    *tm_fail = \&Test::More::fail;

    *bas_ok = \&Test2::Tools::Basic::ok;
}

use vars qw/$TODO/;

sub leg_ok($;$@) {
    my ($bool, $name, @diag);
    my $ctx = context();
    $ctx->ok($bool, $name, \@diag);
    $ctx->release;

    return $bool;
}

sub new_ok($;$@) {
    my ($bool, $name, @diag) = @_;
    my $ctx = context();

    return $ctx->pass_and_release($name) if $bool;
    return $ctx->fail_and_release($name, @diag);
}

{
    local $TODO = "Testing TODO";

    tm_ok(0, "tm_ok fail");
    tm_fail('tm_fail');

    leg_ok(0, "legacy ok fail");
    new_ok(0, "new ok fail");

    bas_ok(0, "basic ok fail");
}

todo new_todo_test => sub {
    tm_ok(0, "tm_ok fail");
    tm_fail('tm_fail');

    leg_ok(0, "legacy ok fail");
    new_ok(0, "new ok fail");

    bas_ok(0, "basic ok fail");
};

done_testing;
