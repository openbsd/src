package ExportTest;

use Filter::Simple;
use base Exporter;

@EXPORT_OK = qw(ok);

FILTER { s/not// };

sub ok { print "ok @_\n" }

1;
