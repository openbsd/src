use Test2::Bundle::Extended -target => 'Test2::Require::EnvVar';

{
    local $ENV{FOO} = 0;
    is($CLASS->skip('FOO'), 'This test only runs if the $FOO environment variable is set', "will skip");

    $ENV{FOO} = 1;
    is($CLASS->skip('FOO'), undef, "will not skip");

    like(
        dies { $CLASS->skip },
        qr/no environment variable specified/,
        "must specify a var"
    );
}

done_testing;
