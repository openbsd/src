use Test2::Bundle::Extended -target => 'Test2::Require::Perl';

is($CLASS->skip('v5.6'), undef, "will not skip");
is($CLASS->skip('v100.100'), 'Perl v100.100.0 required', "will skip"); # fix this before 2054

done_testing;
