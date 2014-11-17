#!./perl -w

BEGIN {
    unless (-d 'blib') {
	chdir 't' if -d 't';
	@INC = '../lib';
    }
    if (!eval "require Socket") {
	print "1..0 # no Socket\n"; exit 0;
    }
    if (ord('A') == 193 && !eval "require Convert::EBCDIC") {
        print "1..0 # EBCDIC but no Convert::EBCDIC\n"; exit 0;
    }
}

use Net::Config;
use Net::NNTP;
use Net::Cmd qw(CMD_REJECT);

unless(@{$NetConfig{nntp_hosts}} && $NetConfig{test_hosts}) {
    print "1..0\n";
    exit;
}

print "1..4\n";

my $i = 1;

$nntp = Net::NNTP->new(Debug => 0)
	or (print("not ok 1\n"), exit);

print "ok 1\n";

my $grp;
foreach $grp (qw(test alt.test control news.announce.newusers)) {
    @grp = $nntp->group($grp);
    last if @grp;
}

if($nntp->status == CMD_REJECT) {
    # Command was rejected, probably because we need authinfo
    map { print "ok ",$_,"\n" } 2,3,4;
    exit;
}

print "not " unless @grp;
print "ok 2\n";


if(@grp && $grp[2] > $grp[1]) {
    $nntp->head($grp[1]) or print "not ";
}
print "ok 3\n";

if(@grp) {
    $nntp->quit or print "not ";
}
print "ok 4\n";

