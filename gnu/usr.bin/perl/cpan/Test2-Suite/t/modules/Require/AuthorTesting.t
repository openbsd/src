use Test2::Bundle::Extended -target => 'Test2::Require::AuthorTesting';

{
    local $ENV{AUTHOR_TESTING} = 0;
    is($CLASS->skip(), 'Author test, set the $AUTHOR_TESTING environment variable to run it', "will skip");

    $ENV{AUTHOR_TESTING} = 1;
    is($CLASS->skip(), undef, "will not skip");
}

done_testing;
