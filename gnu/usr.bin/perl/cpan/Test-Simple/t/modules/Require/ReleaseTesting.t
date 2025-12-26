use Test2::Bundle::Extended -target => 'Test2::Require::ReleaseTesting';

{
    local $ENV{RELEASE_TESTING} = 0;
    is($CLASS->skip(), 'Release test, set the $RELEASE_TESTING environment variable to run it', "will skip");

    $ENV{RELEASE_TESTING} = 1;
    is($CLASS->skip(), undef, "will not skip");
}

done_testing;
