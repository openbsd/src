##########################################################
## Where do I get installed? #############################
##########################################################
INSTALL_PREFIX = /usr/local/
INSTALL_BIN = sbin/
INSTALL_MAN = man/

##########################################################
## Compile time options, These set defaults ##############
##########################################################
# What mail agent should smtpfwdd use?
# This must take arguments of the form: 
# "mailprog -f fromaddr toaddr toaddr ..."
# to deliver mail.
MAIL_AGENT = /usr/sbin/sendmail
#MAIL_AGENT = /usr/lib/sendmail

# What user and group should smtpfwdd and smtpd run as?
# This MUST NOT be root, and must be a user that is "trusted" by 
# sendmail or whatever you are using as MAIL_AGENT above so that the user
# may use the "-f" flag to specify the sender of a mail message.
SMTP_USER = uucp
SMTP_GROUP = uucp

# smtpd and smtpfwdd generate lots of syslogs, by design.
#
# What log facility should smtpd and smtpfwdd use for syslogs? The
# default is LOG_MAIL, which is also used by critters like sendmail.
# if you don't like this and want the logs going elsewhere for easy
# perusal change this to something else (like LOG_LOCAL5) and adjust
# your /etc/syslog.conf file to deal with it to your liking. 
#
# LOG_FACILITY=LOG_LOCAL5
LOG_FACILITY = LOG_MAIL


# Where is the spool directory located?
# This is the directory used by smtpd to chroot to and store
# messages. It is the directory watched for messages by smtpfwdd. This
# directory should be readable and writable only to the user specified
# in SMTP_USER above.
#
# The chroot directory
SPOOLDIR = /usr/spool/smtpd
# Where to store messages, relative to SPOOLDIR above.
SPOOLSUBDIR = .
#Use below instead of above if you don't want smtpd to chroot. This
#isn't normally a good idea if security is your main goal. A nice
#chroot jail may be a pain to build but should give you enough of a
#warm fuzzy to make it worth your while.
#SPOOLDIR = /
#SPOOLSUBDIR = usr/spool/smtpd

# How frequently (in seconds) should smtpfwdd wake up to check the
# spool directory for new mail? 
POLL_TIME = 10

# What is the maximum number of children smtpfwdd should spawn at one time
# when delivering messages before waiting for some to complete? This ensures
# the arrival of hundreds of mail messages doesn't run you out of processes.
MAXCHILDREN = 10

# Should smtpfwdd check the exit code of sendmail? smtpfwdd checks any
# non-zero exit status from sendmail to see if it indicates that
# sendmail thinks the message might be deliverable later. Normally
# this should not be a problem, however if you are not running
# sendmail as your delivery agent, or if your sendmail is broken, the
# exit codes may not mean much. In such a case you may not want
# smtpfwdd to retry message delivery when the mta exits indicating a
# failure. Setting SENDMAIL_RETRY to 0 below will make sure smtpfwdd
# never retries delivery if the MTA returns a non-zero exit
# status. You probably shouldn't change this unless you are sure you
# know what you are doing.
SENDMAIL_RETRY = 1

# Sendmail has a feature/bug that when feeding a message to it it will
# stop if a line contains only a '.'. This is suppressed in smtpfwdd
# by invoking sendmail with the "-oiTrue" option if the mail agent's
# name ends in the string "sendmail". If your "sendmail" isn't really
# sendmail, you may not need or want this. If so, change the SENDMAIL_OITRUE
# below to 0.
SENDMAIL_OITRUE = 1

# Sendmail can handle addresses quoted in <> on the command line. Some
# MTA's (like qmail) can't. set STRIP_QUOTES to 1 if you want smtpfwdd
# to peel off the <> from mail addresses on the command line when 
# invoking the mta
STRIP_QUOTES = 0 

# How many arguments can your execv() call take at once? This can be a
# conservative estimate. It determines the maximum number of
# recipients at a time that MAIL_AGENT will be invoked with by
# smtpfwdd. 
# N.B. MAXARGS must be at least 6, or 5 if SENDMAIL_OITRUE (above) is 1.
# You may wish to change this to be 6 if you want your sendmail to be
# invoked seperately for each receipient.  
MAXARGS = 100

# How long (in seconds) should smtpd block on a read() call to a 
# connected client before giving up on the connection?
READ_TIMEOUT = 600

# Should smtpd check hostnames and ip addresses of a connection
# against the DNS to verify consistency, and report any inconsistencies? 
# Set to 0 for hostchecks, 1 for no hostchecks. Set this to 1 only if
# you don't want any name lookups done.
# NO_HOSTCHECKS = 1
NO_HOSTCHECKS = 0

# Smtpd's default informational status messages in the smtp dialogue
# are somewhat unique and interesting (see smtpd.h). Normally these
# are only seen by people telnetting to your smtp port, or debugging
# mail.  While the author has no shame and won't change the default
# It's understandable that they aren't everyone's cup of tea. Set
# VANILLA_MESSAGES below to 1 and smtp will use plain old politically
# correct Sendmail/RFC 821 style status messages.
# VANILLA_MESSAGES = 1 # Boring....
VANILLA_MESSAGES = 0

# smtpd checks and clobbers and potentially "evil" characters in hostnames,
# and mail addresses recieved on FROM: and RCPT: lines. This clobbers things
# like a connection from hostname `/bin/rm -rf /`@evil.org, or mail to
# "| /bin/sh". It also clobbers things like 8bit chars in such things.
# smtpd always clobbers the characters it doesn't like, 
# and syslogs a note of the fact. The options below determine whether or
# not smtpd will also drop the connection.
#
# Beware of setting this to 1 if your receive mail from sites where
# it's considered ok to put 8 bit ascii chars in message headers. 
# 
# If PARANOID_SMTP is 1, smtpd will close connection on any client
# that puts characters it thinks may be evil in the smtp dialogue, 
# (HELO, FROM, RCPT), or in the message headers. 
# When PARANOID_SMTP is 0, smtpd will replace the characters it thinks
# are evil and continue. 
# PARANOID_SMTP = 1
PARANOID_SMTP = 0

#
# If PARANOID_DNS is 1, smtpd will close connection on any client that
# has characters it thinks may be evil in it's hostname as found by DNS,
# or any client whose DNS forward and reverse mappings are inconsistent
# indicating a DNS spoof of misconfiguration.
# If PARANOID_DNS is 0, smtpd will replace any evil characters it sees
# continue.
# PARANOID_DNS = 1
PARANOID_DNS = 0

# The check above will clobber stuff in the headers from some
# non north-american localizations.
# If your operating system has localization support
# you can define LOCALIZATION below to be your localization.
# For this to work, your operating system must support localization
# with setlocale, and you must copy the appropriate localization
# files into the right place in smtpd's chroot directory.
# The result of this is that smtpd will use a 
# setlocale(LC_CTYPE, LOCALIZATION) to hopefully make sure
# your normal stuff won't get clobbered.
# Leave this set to 0 for no localization support.
# 
#
# SET_LOCALE = 1  # Use a setlocale call to set localization
SET_LOCALE = 0 # don't include localization support at all
#LOCALIZATION = lt_LN.ISO_8859-1
LOCALIZATION = C
#LOCALIZATION = POSIX 
#LOCALIZATION = ISO-8859-1 
#LOCALIZATION = KOI-8 

# Some sites may wish to ensure smtpd does *not* get run with command
# line options to affect the compiled-in behaviours. Set
# NO_COMMANDLINE_OPTIONS to 1 to make smtpd and smtpfwdd ignore any command
# line options.
#NO_COMMANDLINE_OPTIONS=1
NO_COMMANDLINE_OPTIONS=0

# Smtpd does not support ESMTP's EHLO command normally, as it shouldn't
# need to. According to RFC, if the EHLO is unrecognized the connecting
# agent should drop back to a HELO on the second attempt (and then be
# talking vanilla smtp). Unfortunately Netscape Communicator betas seem
# seem to have a bug in which they simply try the EHLO again. Sigh. 
# setting EHLO_KLUDGE to 1 will make smtpd accept a second EHLO as a helo, 
# thereby kludging around this bug in Communicator. 
#EHLO_KLUDGE=1  
EHLO_KLUDGE=0  


# smtpd can check FROM and RCPT addresses, along with the connecting
# host info using an address check file. This can be used to only
# allow certain mail addresses on a FROM:, or certain combinations of
# FROM: and RCPT from certain hosts. It can be used to prevent third
# party relays, enforce outgoing address conventions, prevent outgoing
# SPAM/obvious forgeries, or block incoming SPAM. Setting this to 0 will
# mean that none of the address checking functionality is compiled in at
# all. (meaning all of address_check.c is #ifdefed out).  
CHECK_ADDRESS = 1
# CHECK_ADDRESS = 0

# Set This to 1 if you are running smtpd on a Juniper firewall machine.
# This enables smtpd to use juniper's trusted/untrusted interface
# mechanism, allowing you to use the UNTRUSTED or TRUSTED specials
# in address check rules to match based on what kind of interface
# a connection arrived on.
#
# You must set this to 0 this on a non-juniper machine or smtpd will not
# build with CHECK_ADDRESS set to 1. You will get an error about
# juniper_firewall.h not existing if you forget this.
#
JUNIPER_SUPPORT=1
#JUNIPER_SUPPORT=0

# Use regexp's in patterns?  If you have a POSIX <regex.h> and friends, 
# and you trust the regex lib enough for use, you can set USE_REGEX to 
# 1, this allows you to specify a pattern enclosed in "/" as a regular
# POSIX/henry-spencer style extended regex for case insensitive matching
# (what's between the "/" gets fed to regcomp with 
# REG_EXTENDED|REG_ICASE|REG_NOSUB options, then matched against) 
#
# Note that older beasts (like SunOS 4.X) usually don't have a regex
# library, so if you're using something that doesn't you should set this
# to 0.
#
#USE_REGEX=0
USE_REGEX=1

# Enable checking namservers?  with NS_MATCH set to 1, patterns of the
# form NS=pattern can be used to match rules against the nameservers
# and MX records of originating connections or mail addresses, rather
# than the connection itself. This allows for small rules to block out
# whole blocks of bozos from rogue providers such as ispam.net should
# you choose to do so. i.e.
#
#noto_delay:NS=205.199.212.0/24 NS=205.199.2.0/24 NS=207.124.161.0/24:ALL:ALL
#noto_delay:ALL:NS=205.199.212.0/24 NS=205.199.2.0/24 NS=207.124.161.0/24:ALL
#
# would effectively block off anything originiating from, or with a FROM:
# address looking like any of it's nameservers are on one of cyberpromo.com's
# addresses. 
#
# uncomment the LD_LIBS line as well if you enable NS_MATCH unless your 
# resolver routines are in libc (like they are on some Linuxes)
#
# The NS_MATCH feature is lovingly dedicated by Bob Beck to Spamford
# Wallace and ispam.net
#
#NS_MATCH=0
NS_MATCH=1
LD_LIBS=-lresolv  # you may or may not need this.

# The rules file for address checking, if enabled, remember this file
# will be in the chroot jail, so the line below probably means
# /usr/spool/smtpd/etc/smtpd_check_rules, unless you changed the
# chroot directory above.
CHECK_FILE = /etc/smtpd_check_rules

# address checking rules may want user information, gotten from an RFC
# 931 style ident. This info may be passed in the environment to smtpd
# (from juniperd or the tcp wrapper), or smtpd will do the ident
# request itself if required.  CHECK_IDENT determines the timeout (in
# seconds) on an ident request.  if CHECK_IDENT is 0, no ident
# requests will be made by smtpd even if it sees a rule that would
# normally make it perform one.  
# CHECK_IDENT = 0 
CHECK_IDENT = 10

# If you use the address checks to block incoming mail from certain
# spam sites, you can set NOTO_DELAY and DENY_DELAY below to specify
# the time in seconds smtpd will go to sleep for after matching a
# "noto_delay" or "deny_delay" rule.  This makes your site a small
# headache to the spammer since they have to wait before being denied,
# instead of immediately knowing so, and proceeding on to their next
# victim.
# NOTO_DELAY = 0
# DENY_DELAY = 0 
# NOTO_DELAY = 300
# DENY_DELAY = 300
NOTO_DELAY = 50
DENY_DELAY = 50

# Because CHECK_ADDRESS above is meant to be used as a nuisance filter
# the default is to ALLOW on failure rather than deny service when no
# match is found, or if something happens while attempting to match a
# rule (such as a system call failure, or you make a syntax error in the
# rules file). Change
# CHECK_ADDRESS_DENY_ON_FAILURE to 1 to have smtpd not allow anything
# not explicitly allowed by the rules.
# CHECK_ADDRESS_DENY_ON_FAILURE = 1
CHECK_ADDRESS_DENY_ON_FAILURE = 0


# Options to pass to indent when tidying up the code.
INDENT_ARGS = -di0 -br -bad -bap -nbc -cdb -ce -i2 -lp -npro -npcs -psl

##########################################################
## End of compile time options. ##########################
## Select your Operating system below this.. ############# 
##########################################################


OPTIONS = -DMAIL_AGENT=\"$(MAIL_AGENT)\" -DSMTP_USER=\"$(SMTP_USER)\" \
	-DSMTP_GROUP=\"$(SMTP_GROUP)\" \
	-DLOG_FACILITY=$(LOG_FACILITY) -DVANILLA_MESSAGES=$(VANILLA_MESSAGES) \
	-DSPOOLDIR=\"$(SPOOLDIR)\" -DSPOOLSUBDIR=\"$(SPOOLSUBDIR)\" \
	-DPOLL_TIME=$(POLL_TIME) -DSENDMAIL_RETRY=$(SENDMAIL_RETRY) \
	-DSENDMAIL_OITRUE=$(SENDMAIL_OITRUE) -DSTRIP_QUOTES=$(STRIP_QUOTES) \
	-DMAXCHILDREN=$(MAXCHILDREN) -DMAXARGS=$(MAXARGS) \
	-DREAD_TIMEOUT=$(READ_TIMEOUT) -DNO_HOSTCHECKS=$(NO_HOSTCHECKS) \
	-DPARANOID_SMTP=$(PARANOID_SMTP) -DPARANOID_DNS=$(PARANOID_DNS) \
	-DNO_COMMANDLINE_OPTIONS=$(NO_COMMANDLINE_OPTIONS) \
	-DEHLO_KLUDGE=$(EHLO_KLUDGE) \
	-DCHECK_ADDRESS=$(CHECK_ADDRESS) -DCHECK_FILE=\"$(CHECK_FILE)\" \
	-DCHECK_ADDRESS_DENY_ON_FAILURE=$(CHECK_ADDRESS_DENY_ON_FAILURE) \
	-DCHECK_IDENT=$(CHECK_IDENT) \
	-DNOTO_DELAY=$(NOTO_DELAY) -DDENY_DELAY=$(DENY_DELAY) \
	-DSET_LOCALE=$(SET_LOCALE) -DLOCALIZATION=\"$(LOCALIZATION)\" \
	-DJUNIPER_SUPPORT=$(JUNIPER_SUPPORT) -DNS_MATCH=$(NS_MATCH) \
	-DUSE_REGEX=$(USE_REGEX)

## CC of choice.
#CC = cc
CC = gcc

####################################################
###### Uncomment your Operating System below #######
####################################################

# The Source is with you (*BSD 4.4, Linux)
CFLAGS = -g -O -Wall -DUSE_FLOCK -DUSE_MKSTEMP $(OPTIONS)


#SunOS 5.X
#CFLAGS = -g -O -Wall -DUSE_LOCKF -DNEEDS_FCNTL_H -DNEEDS_STRINGS_H -DBROKEN_SUN_INCLUDES $(OPTIONS)
#LD_LIBS = -lsocket -lnsl -lresolv

#SunOS 4.X. No regex lib here, so you will have to set USE_REGEX to 0
#above unless you have built the regex lib seperately.
#CFLAGS = -g -O -DUSE_FLOCK -DNO_MEMMOVE -DBSD_SIGNAL -DSUNOS_GETOPT $(OPTIONS)

#Irix 
#(gcc doesn't like -g here)
#CFLAGS = -Wall -O -DGETOPT_EOF -DUSE_FLOCK -DNEEDS_FCNTL_H -DNEEDS_BSTRING_H -DIRIX_BROKEN_INCLUDES $(OPTIONS)

#AIX
#CFLAGS = -g -O -Wall -DGETOPT_EOF -DUSE_LOCKF -DNEEDS_FCNTL_H -DNEEDS_LOCKF_H -DNEEDS_STRINGS_H -DNEEDS_SELECT_H  $(OPTIONS)


all: smtpd smtpfwdd

smtpd:	smtpd.o address_check.o accumlog.o
	$(CC) -o smtpd smtpd.o address_check.o $(LD_LIBS)

smtpfwdd:	smtpfwdd.o accumlog.o
	$(CC) -o smtpfwdd smtpfwdd.o $(LD_LIBS)

smtpd.o:	smtpd.c smtpd.h smtp.h 

smtpfwdd.o:	smtpfwdd.c smtp.h

accumlog.o:	accumlog.c

indent:
	indent $(INDENT_ARGS) smtpd.c
	indent $(INDENT_ARGS) smtpfwdd.c
	indent $(INDENT_ARGS) address_check.c

install:	smtpd smtpfwdd
	install -m 500 smtpd $(INSTALL_PREFIX)$(INSTALL_BIN)smtpd
	install -m 500 smtpfwdd $(INSTALL_PREFIX)$(INSTALL_BIN)smtpfwdd

clean:
	/bin/rm -f  *.o 

clobber:
	/bin/rm -f  smtpd smtpfwdd *.o 


