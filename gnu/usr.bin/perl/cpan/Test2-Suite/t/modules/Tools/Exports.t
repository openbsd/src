use Test2::Bundle::Extended -target => 'Test2::Tools::Exports';

{
    package Temp;
    use Test2::Tools::Exports;

    imported_ok(qw/imported_ok not_imported_ok/);
    not_imported_ok(qw/xyz/);
}

like(
    intercept { imported_ok('x') },
    array {
        fail_events Ok => { pass => 0 };
        event Diag => { message => "'x' was not imported." };
        end;
    },
    "Failed, x is not imported"
);

like(
    intercept { not_imported_ok('ok') },
    array {
        fail_events Ok => { pass => 0 };
        event Diag => { message => "'ok' was imported." };
        end;
    },
    "Failed, 'ok' is imported"
);

done_testing;
