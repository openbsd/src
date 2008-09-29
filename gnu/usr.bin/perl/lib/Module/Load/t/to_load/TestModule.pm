package TestModule;

use strict;
require Exporter;
use vars qw(@EXPORT @EXPORT_OK @ISA);

@ISA = qw(Exporter);
@EXPORT = qw(func2);
@EXPORT_OK = qw(func1);

sub func1 { return "func1"; }

sub func2 { return "func2"; }

1;
