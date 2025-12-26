use Test2::Bundle::Extended -target => 'Test2::Tools::Encoding';

require Test2::Formatter::TAP;

use File::Temp qw/tempfile/;

{
    package Temp;
    use Test2::Tools::Encoding;

    main::imported_ok(qw/set_encoding/);
}

my $warnings;
intercept {
    $warnings = warns {
        use utf8;

        my ($fh, $name);
        my $ct = 100;
        until ($fh) {
            --$ct or die "Failed to get temp file after 100 tries";
            ($fh, $name) = eval { tempfile() };
        }

        Test2::API::test2_stack->top->format(
            Test2::Formatter::TAP->new(
                handles => [$fh, $fh, $fh],
            ),
        );

        set_encoding('utf8');
        ok(1, '†');

        unlink($name) or print STDERR "Could not remove temp file $name: $!\n";
    };
};

ok(!$warnings, "set_encoding worked");

my $exception;
intercept {
    $exception = dies {
        set_encoding('utf8');
    };
};

like(
    $exception,
    qr/Unable to set encoding on formatter '<undef>'/,
    "Cannot set encoding without a formatter"
);

done_testing;
