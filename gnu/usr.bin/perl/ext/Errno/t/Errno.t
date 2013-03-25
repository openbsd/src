#!./perl -w

use Test::More tests => 12;

# Keep this before the use Errno.
my $has_einval = exists &Errno::EINVAL;

BEGIN {
    use_ok("Errno");
}

BAIL_OUT("No errno's are exported") unless @Errno::EXPORT_OK;

my $err = $Errno::EXPORT_OK[0];
my $num = &{"Errno::$err"};

is($num, &{"Errno::$err"});

$! = $num;
ok(exists $!{$err});

$! = 0;
ok(! $!{$err});

ok(join(",",sort keys(%!)) eq join(",",sort @Errno::EXPORT_OK));

eval { exists $!{[]} };
ok(! $@);

eval {$!{$err} = "qunckkk" };
like($@, qr/^ERRNO hash is read only!/);

eval {delete $!{$err}};
like($@, qr/^ERRNO hash is read only!/);

# The following tests are in trouble if some OS picks errno values
# through Acme::MetaSyntactic::batman
is($!{EFLRBBB}, "");
ok(! exists($!{EFLRBBB}));

SKIP: {
    skip("Errno does not have EINVAL", 1)
	unless grep {$_ eq 'EINVAL'} @Errno::EXPORT_OK;
    is($has_einval, 1,
       'exists &Errno::EINVAL compiled before Errno is loaded works fine');
}

SKIP: {
    skip("Errno does not have EBADF", 1)
	unless grep {$_ eq 'EBADF'} @Errno::EXPORT_OK;
    is(exists &Errno::EBADF, 1,
       'exists &Errno::EBADF compiled after Errno is loaded works fine');
}
