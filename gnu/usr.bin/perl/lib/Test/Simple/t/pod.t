#!/usr/bin/perl -w
# $Id: pod.t,v 1.1 2009/05/16 21:42:57 simon Exp $

use Test::More;
eval "use Test::Pod 1.00";
plan skip_all => "Test::Pod 1.00 required for testing POD" if $@;
all_pod_files_ok();
