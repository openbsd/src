BEGIN {
        chdir 't' if -d 't';
        @INC = '../lib';
}

print "1..1\n";

require MIME::Base64;

eval {
    MIME::Base64::encode(v300);
};

print "not " unless $@;
print "ok 1\n";

