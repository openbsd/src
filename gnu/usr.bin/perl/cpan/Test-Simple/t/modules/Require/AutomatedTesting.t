use Test2::Bundle::Extended -target => 'Test2::Require::AutomatedTesting';

{
    local $ENV{AUTOMATED_TESTING} = 0;
    is($CLASS->skip(), 'Automated test, set the $AUTOMATED_TESTING environment variable to run it', "will skip");

    $ENV{AUTOMATED_TESTING} = 1;
    is($CLASS->skip(), undef, "will not skip");
}

done_testing;
