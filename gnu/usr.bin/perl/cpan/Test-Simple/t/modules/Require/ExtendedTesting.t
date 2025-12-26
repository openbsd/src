use Test2::Bundle::Extended -target => 'Test2::Require::ExtendedTesting';

{
    local $ENV{EXTENDED_TESTING} = 0;
    is($CLASS->skip(), 'Extended test, set the $EXTENDED_TESTING environment variable to run it', "will skip");

    $ENV{EXTENDED_TESTING} = 1;
    is($CLASS->skip(), undef, "will not skip");
}

done_testing;
