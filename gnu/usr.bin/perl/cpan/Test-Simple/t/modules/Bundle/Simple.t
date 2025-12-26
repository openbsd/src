use strict;
use warnings;
use Test2::Bundle::Simple;
use Test2::Tools::Exports;

imported_ok qw/ok plan done_testing skip_all/;

ok(Test2::Plugin::ExitSummary->active, "Exit Summary is loaded");

done_testing;

1;

__END__

