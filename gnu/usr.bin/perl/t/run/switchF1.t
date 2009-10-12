#!perl -w
print "1..5\n";

my $file = "Run_switchF1.pl";

open F, ">$file" or die "Open $file: $!";

my $prog = <<'EOT';
#!./perl -anF[~#QQ\\xq']

BEGIN {
    *ARGV = *DATA;
}
print "@F";

__DATA__
okx1
okq2
ok\3
ok'4
EOT

# 2 of the characters toke.c used to use to quote the split parameter:
$prog =~ s/QQ/\x01\x80/;
# These 2 plus ~ # and ' were enough to make perl choke
print F $prog;
close F or die "Close $file: $!";

print system ($^X, $file) ? "not ok 5\n" : "ok 5\n";

unlink $file or die "Unlink $file: $!";
