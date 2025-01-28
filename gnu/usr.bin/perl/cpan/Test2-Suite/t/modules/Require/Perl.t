use Test2::Bundle::Extended -target => 'Test2::Require::Perl';

is($CLASS->skip('v5.6'), undef, "will not skip");
is($CLASS->skip('v10.10'), 'Perl v10.10.0 required', "will skip");

done_testing;
