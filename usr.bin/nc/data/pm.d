# obligatory duplicate of dr delete's Livingston portmaster crash, aka
# telnet break.  Fire into its telnet listener.  An *old* bug by now, but
# consider the small window one might obtain from a slightly out-of-rev PM
# used as a firewall, that starts routing IP traffic BEFORE its filter sets
# are fully loaded...

255 # 0xff # . 1
243 # 0xf3 # . 2
