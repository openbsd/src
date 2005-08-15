divert(-1)
#
# Default OpenBSD sendmail configuration for systems accepting mail
# from the internet.
#
# Note that lines beginning with "dnl" below are comments.

divert(0)dnl
VERSIONID(`@(#)openbsd-proto.mc $Revision: 1.11 $')dnl
OSTYPE(openbsd)dnl
dnl
dnl If you have a non-static IP address you may wish to forward outgoing mail
dnl through your ISP's mail server to prevent matching one of the dialup
dnl DNS black holes.  Just uncomment the following line and replace
dnl mail.myisp.net with the hostname of your ISP's mail server.
dnl
dnl define(`SMART_HOST', `mail.myisp.net')dnl
dnl
dnl Disable EXPN and VRFY to help thwart address harvesters and require
dnl senders to say hello.
dnl
define(`confPRIVACY_FLAGS', `authwarnings,needmailhelo,noexpn,novrfy,nobodyreturn')dnl
dnl
dnl We wish to make the existence of the local-host-names and
dnl trusted-users files optional, hence the "-o" below.
dnl
define(`confCW_FILE', `-o MAIL_SETTINGS_DIR`'local-host-names')dnl
define(`confCT_FILE', `-o MAIL_SETTINGS_DIR`'trusted-users')dnl
dnl
dnl Use of UUCP-style addresses in the modern internet are generally
dnl an error (and sometimes used by spammers) so disable support for them.
dnl To simply treat '!' as a normal character, change `reject' to
dnl `nospecial'.
dnl
FEATURE(nouucp, `reject')dnl
dnl
dnl The access database allows for certain actions to be taken based on
dnl the source address.
dnl
FEATURE(`access_db', `hash -o -T<TMPF> /etc/mail/access')dnl
FEATURE(`blacklist_recipients')dnl
dnl
dnl Enable support for /etc/mail/local-host-names.
dnl Contains hostnames that should be considered local.
dnl
FEATURE(`use_cw_file')dnl
dnl
dnl Enable support for /etc/mail/mailertable.
dnl
FEATURE(`mailertable', `hash -o /etc/mail/mailertable')dnl
dnl
dnl Enable support for /etc/mail/trusted-users.
dnl Users listed herein may spoof mail from other users.
dnl
FEATURE(`use_ct_file')dnl
dnl
dnl Enable support for /etc/mail/virtusertable.
dnl Used to do N -> N address mapping.
dnl
FEATURE(`virtusertable', `hash -o /etc/mail/virtusertable')dnl
dnl
dnl Rewrite (unqualified) outgoing email addresses using the
dnl mapping listed in /etc/mail/genericstable
dnl
FEATURE(genericstable, `hash -o /etc/mail/genericstable')dnl
dnl
dnl Normally only local addresses are rewritten.  By using
dnl generics_entire_domain and either GENERICS_DOMAIN
dnl or GENERICS_DOMAIN_FILE addresses from hosts in the
dnl specified domain(s) will be rewritten too.
dnl
dnl FEATURE(generics_entire_domain)dnl
dnl GENERICS_DOMAIN(`othercompany.com')dnl
dnl GENERICS_DOMAIN_FILE(`/etc/mail/generics-domains')dnl
dnl
dnl Include the local host domain even on locally delivered mail
dnl (which would otherwise contain only the username).
FEATURE(always_add_domain)dnl
dnl
dnl Bounce messages addressed to "address.REDIRECT".  This allows the
dnl admin to alias a user who has moved to "new_address.REDIRECT" so
dnl that senders will know the user's new address.
FEATURE(redirect)dnl
dnl
dnl Accept incoming connections on any IPv4 or IPv6 interface for ports
dnl 25 (SMTP) and 587 (MSA).
dnl
FEATURE(`no_default_msa')dnl
DAEMON_OPTIONS(`Family=inet, Address=0.0.0.0, Name=MTA')dnl
DAEMON_OPTIONS(`Family=inet6, Address=::, Name=MTA6, M=O')dnl
DAEMON_OPTIONS(`Family=inet, Address=0.0.0.0, Port=587, Name=MSA, M=E')dnl
DAEMON_OPTIONS(`Family=inet6, Address=::, Port=587, Name=MSA6, M=O, M=E')dnl
dnl
dnl Use either IPv4 or IPv6 for outgoing connections.
dnl
CLIENT_OPTIONS(`Family=inet, Address=0.0.0.0')dnl
CLIENT_OPTIONS(`Family=inet6, Address=::')dnl
dnl
dnl Some broken nameservers will return SERVFAIL (a temporary failure)
dnl on T_AAAA (IPv6) lookups.
dnl
define(`confBIND_OPTS', `WorkAroundBrokenAAAA')dnl
dnl
dnl TLS/SSL support; uncomment and read starttls(8) to use.
dnl
dnl define(`CERT_DIR', `MAIL_SETTINGS_DIR`'certs')dnl
dnl define(`confCACERT_PATH', `CERT_DIR')dnl
dnl define(`confCACERT', `CERT_DIR/mycert.pem')dnl
dnl define(`confSERVER_CERT', `CERT_DIR/mycert.pem')dnl
dnl define(`confSERVER_KEY', `CERT_DIR/mykey.pem')dnl
dnl define(`confCLIENT_CERT', `CERT_DIR/mycert.pem')dnl
dnl define(`confCLIENT_KEY', `CERT_DIR/mykey.pem')dnl
dnl
dnl Masquerading -- rewriting the From address to a specific domain.
dnl Please see the "MASQUERADING AND RELAYING" section of
dnl /usr/share/sendmail/README for details.
dnl
dnl MASQUERADE_AS(`mycompany.com')dnl
dnl
dnl Masquerade the envelope From in addition to the From: header.
dnl
dnl FEATURE(masquerade_envelope)dnl
dnl
dnl Masquerade host.sub.dom.ain as well as host.dom.ain.
dnl
dnl FEATURE(masquerade_entire_domain)dnl
dnl
dnl Only masquerade messages going outside the local domain.
dnl
dnl FEATURE(local_no_masquerade)dnl
dnl
dnl Rewrite addresses from user@othercompany.com when relayed in
dnl addition to locally-generated messages.
dnl
dnl MASQUERADE_DOMAIN(`othercompany.com')dnl
dnl
dnl Specific hosts that should be excepted from MASQUERADE_DOMAIN.
dnl
dnl MASQUERADE_EXCEPTION(`host.othercompany.com')dnl
dnl
dnl Only masquerade for hosts listed by MASQUERADE_DOMAIN
dnl (normally any host considered local is also masqueraded).
dnl
dnl FEATURE(limited_masquerade)dnl
dnl
dnl Specific users that should be excepted from masquerading.
dnl
dnl EXPOSED_USER(`root')dnl
dnl EXPOSED_USER(`daemon')dnl
dnl EXPOSED_USER_FILE(`/etc/mail/exposed-users')dnl
dnl
dnl End of masquerading section.
MAILER(local)dnl
MAILER(smtp)dnl
dnl
dnl Enforce valid Message-Id to help stop spammers.
dnl
LOCAL_RULESETS
HMessage-Id: $>CheckMessageId

SCheckMessageId
R< $+ @ $+ >		$@ OK
R$*			$#error $: 553 Header Error
