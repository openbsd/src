use Test2::Bundle::Extended;
use Test2::Tools::Spec;

# Get a non-canon context for the root hub.
my $ctx = sub {
    my $ctx = context();
    my $out = $ctx->snapshot;
    $ctx->release;
    return $out;
}->();

tests foo => sub {
    # This ok is part of the subtest and goes to the subtest hub
    ok(1, "pass");

    # Use the non-canon root hub context to set a plan. We do this here so that
    # no plan is ever set if the test block does not run.
    $ctx->plan(1);
};

# done_testing intentionally omitted, see #3
