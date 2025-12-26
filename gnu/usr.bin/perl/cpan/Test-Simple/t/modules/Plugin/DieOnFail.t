use Test2::Bundle::Extended;

use Test2::Plugin::DieOnFail;

my $error;
like(
    intercept {
        ok(1, "pass");
        $error = dies {
            ok(0, "fail");
            ok(1, "Should not see");
        };
    },
    array {
        filter_items { grep { !$_->isa('Test2::Event::Diag') } @_ };
        event Ok => { name => "pass", pass => 1 };
        event Ok => { name => "fail", pass => 0 };
        end;
    },
    "Died after the failure"
);

like(
    $error,
    qr/\(Die On Fail\)/,
    "Got the error"
);

sub mok {
    my ($ok, $name) = @_;
    my $ctx = context();
    ok($ok, $name);
    diag "Should see this after failure";
    $ctx->release;
    return $ok;
}

$error = undef;
like(
    intercept {
        ok(1, "pass");
        $error = dies {
            mok(0, "fail");
            ok(1, "Should not see");
        };
    },
    array {
        event Ok => { name => "pass", pass => 1 };
        event Ok => { name => "fail", pass => 0 };
        event Diag => {}; # Typical failure diag
        event Diag => { message => "Should see this after failure" };
        end;
    },
    "Tool had time to output the diag"
);

like(
    $error,
    qr/\(Die On Fail\)/,
    "Got the error"
);

done_testing;
