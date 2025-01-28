use Test2::Bundle::Extended -target => 'Test2::AsyncSubtest::Hub';
use Test2::AsyncSubtest::Hub;

isa_ok($CLASS, 'Test2::Hub::Subtest');

ok(!$CLASS->can('inherit')->(), "inherit does nothing");

done_testing;
