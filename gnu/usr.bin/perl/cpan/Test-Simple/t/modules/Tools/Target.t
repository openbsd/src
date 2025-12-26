use Test2::Bundle::Extended;

use Test2::Tools::Target 'Test2::Tools::Target';

is($CLASS, 'Test2::Tools::Target', "set default var");
is(CLASS(), 'Test2::Tools::Target', "set default const");

use Test2::Tools::Target FOO => 'Test2::Tools::Target';

is($FOO,  'Test2::Tools::Target', "set custom var");
is(FOO(), 'Test2::Tools::Target', "set custom const");

done_testing;
