use Test2::Bundle::Extended -target => 'Test2::Require::NonInteractiveTesting';

{
    local $ENV{NONINTERACTIVE_TESTING} = 0;
    is($CLASS->skip(), 'NonInteractive test, set the $NONINTERACTIVE_TESTING environment variable to run it', "will skip");

    $ENV{NONINTERACTIVE_TESTING} = 1;
    is($CLASS->skip(), undef, "will not skip");
}

done_testing;
