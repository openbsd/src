#!./perl

# quickie tests to see if h2ph actually runs and does more or less what is
# expected

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

my $extracted_program = '../utils/h2ph'; # unix, nt, ...
if ($^O eq 'VMS') { $extracted_program = '[-.utils]h2ph.com'; }
if (!(-e $extracted_program)) {
    print "1..0 # Skip: $extracted_program was not built\n";
    exit 0;
}

print "1..2\n";

# quickly compare two text files
sub txt_compare {
    local ($/, $A, $B);
    for (($A,$B) = @_) { open(_,"<$_") ? $_ = <_> : die "$_ : $!"; close _ }
    $A cmp $B;
}

# does it run?
$ok = system("$^X \"-I../lib\" $extracted_program -d. \"-Q\" lib/h2ph.h");
print(($ok == 0 ? "" : "not "), "ok 1\n");
    
# does it work? well, does it do what we expect? :-)
$ok = txt_compare("lib/h2ph.ph", "lib/h2ph.pht");
print(($ok == 0 ? "" : "not "), "ok 2\n");
    
# cleanup - should this be in an END block?
unlink("lib/h2ph.ph");
unlink("_h2ph_pre.ph");
