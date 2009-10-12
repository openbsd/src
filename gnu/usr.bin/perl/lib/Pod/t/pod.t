#!/usr/bin/perl
#
# t/pod.t -- Test POD formatting.

eval 'use Test::Pod 1.00';
if ($@) {
    print "1..1\n";
    print "ok 1 # skip - Test::Pod 1.00 required for testing POD\n";
    exit;
}
all_pod_files_ok ();
