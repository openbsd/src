package NoExporter;
# $Id: NoExporter.pm,v 1.1 2009/05/16 21:42:58 simon Exp $

$VERSION = 1.02;

sub import {
    shift;
    die "NoExporter exports nothing.  You asked for: @_" if @_;
}

1;

