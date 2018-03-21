#	$OpenBSD: trimcerts.awk,v 1.1 2018/03/21 18:53:24 sthen Exp $

#
#	read in a formatted list of X509 certificates with long decodes,
#	output only short comments plus the certificates themselves
#

BEGIN {
	if (ARGC != 3) {
		print "usage: awk -f trimcert.awk cert.pem outputfile";
		bad=1;
		exit 1;
	}
	ARGC=2;
	incert=0;
}

{
	if ($0 ~ /^-----BEGIN CERTIFICATE-----/) {
		incert=1;
	}
	if ($0 ~ /^#/ || incert) {
		print $0 > ARGV[2];
	}
	if ($0 ~ /^-----END CERTIFICATE-----/) {
		incert=0;
	}
}

END {
	if (!bad) {
		system("chmod 444 " ARGV[2]);
		system("chown root:bin " ARGV[2]);
	}
}
