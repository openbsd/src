#define TRUSTED_KEYS "\
# The bind.keys file is used to override the built-in DNSSEC trust anchors\n\
# which are included as part of BIND 9.  As of the current release, the only\n\
# trust anchors it contains are those for the DNS root zone (\".\"), and for\n\
# the ISC DNSSEC Lookaside Validation zone (\"dlv.isc.org\").  Trust anchors\n\
# for any other zones MUST be configured elsewhere; if they are configured\n\
# here, they will not be recognized or used by named.\n\
#\n\
# The built-in trust anchors are provided for convenience of configuration.\n\
# They are not activated within named.conf unless specifically switched on.\n\
# To use the built-in root key, set \"dnssec-validation auto;\" in\n\
# named.conf options.  To use the built-in DLV key, set\n\
# \"dnssec-lookaside auto;\".  Without these options being set,\n\
# the keys in this file are ignored.\n\
#\n\
# This file is NOT expected to be user-configured.\n\
#\n\
# These keys are current as of Feburary 2017.  If any key fails to\n\
# initialize correctly, it may have expired.  In that event you should\n\
# replace this file with a current version.  The latest version of\n\
# bind.keys can always be obtained from ISC at https://www.isc.org/bind-keys.\n\
\n\
trusted-keys {\n\
        # ISC DLV: See https://www.isc.org/solutions/dlv for details.\n\
        #\n\
        # NOTE: The ISC DLV zone is being phased out as of February 2017;\n\
        # the key will remain in place but the zone will be otherwise empty.\n\
        # Configuring \"dnssec-lookaside auto;\" to activate this key is\n\
        # harmless, but is no longer useful and is not recommended.\n\
        dlv.isc.org. 257 3 5 \"BEAAAAPHMu/5onzrEE7z1egmhg/WPO0+juoZrW3euWEn4MxDCE1+lLy2\n\
                brhQv5rN32RKtMzX6Mj70jdzeND4XknW58dnJNPCxn8+jAGl2FZLK8t+\n\
                1uq4W+nnA3qO2+DL+k6BD4mewMLbIYFwe0PG73Te9fZ2kJb56dhgMde5\n\
                ymX4BI/oQ+cAK50/xvJv00Frf8kw6ucMTwFlgPe+jnGxPPEmHAte/URk\n\
                Y62ZfkLoBAADLHQ9IrS2tryAe7mbBZVcOwIeU/Rw/mRx/vwwMCTgNboM\n\
                QKtUdvNXDrYJDSHZws3xiRXF1Rf+al9UmZfSav/4NWLKjHzpT59k/VSt\n\
                TDN0YUuWrBNh\";\n\
\n\
        # ROOT KEYS: See https://data.iana.org/root-anchors/root-anchors.xml\n\
        # for current trust anchor information.\n\
        #\n\
        # These keys are activated by setting \"dnssec-validation auto;\"\n\
        # in named.conf.\n\
        #\n\
        # This key (19036) is to be phased out starting in 2017. It will\n\
        # remain in the root zone for some time after its successor key\n\
        # has been added. It will remain this file until it is removed from\n\
        # the root zone.\n\
        . 257 3 8 \"AwEAAagAIKlVZrpC6Ia7gEzahOR+9W29euxhJhVVLOyQbSEW0O8gcCjF\n\
                FVQUTf6v58fLjwBd0YI0EzrAcQqBGCzh/RStIoO8g0NfnfL2MTJRkxoX\n\
                bfDaUeVPQuYEhg37NZWAJQ9VnMVDxP/VHL496M/QZxkjf5/Efucp2gaD\n\
                X6RS6CXpoY68LsvPVjR0ZSwzz1apAzvN9dlzEheX7ICJBBtuA6G3LQpz\n\
                W5hOA2hzCTMjJPJ8LbqF6dsV6DoBQzgul0sGIcGOYl7OyQdXfZ57relS\n\
                Qageu+ipAdTTJ25AsRTAoub8ONGcLmqrAmRLKBP1dfwhYB4N7knNnulq\n\
                QxA+Uk1ihz0=\";\n\
\n\
        # This key (20326) is to be published in the root zone in 2017.\n\
        # Servers which were already using the old key (19036) should\n\
        # roll seamlessly to this new one via RFC 5011 rollover. Servers\n\
        # being set up for the first time can use the contents of this\n\
        # file as initializing keys; thereafter, the keys in the\n\
        # managed key database will be trusted and maintained\n\
        # automatically.\n\
        . 257 3 8 \"AwEAAaz/tAm8yTn4Mfeh5eyI96WSVexTBAvkMgJzkKTOiW1vkIbzxeF3\n\
                +/4RgWOq7HrxRixHlFlExOLAJr5emLvN7SWXgnLh4+B5xQlNVz8Og8kv\n\
                ArMtNROxVQuCaSnIDdD5LKyWbRd2n9WGe2R8PzgCmr3EgVLrjyBxWezF\n\
                0jLHwVN8efS3rCj/EWgvIWgb9tarpVUDK/b58Da+sqqls3eNbuv7pr+e\n\
                oZG+SrDK6nWeL3c6H5Apxz7LjVc1uTIdsIXxuOLYA4/ilBmSVIzuDWfd\n\
                RUfhHdY6+cn8HFRm+2hM8AnXGXws9555KrUB5qihylGa8subX2Nn6UwN\n\
                R1AkUTV74bU=\";\n\
};\n\
"

#define MANAGED_KEYS "\
# The bind.keys file is used to override the built-in DNSSEC trust anchors\n\
# which are included as part of BIND 9.  As of the current release, the only\n\
# trust anchors it contains are those for the DNS root zone (\".\"), and for\n\
# the ISC DNSSEC Lookaside Validation zone (\"dlv.isc.org\").  Trust anchors\n\
# for any other zones MUST be configured elsewhere; if they are configured\n\
# here, they will not be recognized or used by named.\n\
#\n\
# The built-in trust anchors are provided for convenience of configuration.\n\
# They are not activated within named.conf unless specifically switched on.\n\
# To use the built-in root key, set \"dnssec-validation auto;\" in\n\
# named.conf options.  To use the built-in DLV key, set\n\
# \"dnssec-lookaside auto;\".  Without these options being set,\n\
# the keys in this file are ignored.\n\
#\n\
# This file is NOT expected to be user-configured.\n\
#\n\
# These keys are current as of Feburary 2017.  If any key fails to\n\
# initialize correctly, it may have expired.  In that event you should\n\
# replace this file with a current version.  The latest version of\n\
# bind.keys can always be obtained from ISC at https://www.isc.org/bind-keys.\n\
\n\
managed-keys {\n\
        # ISC DLV: See https://www.isc.org/solutions/dlv for details.\n\
        #\n\
        # NOTE: The ISC DLV zone is being phased out as of February 2017;\n\
        # the key will remain in place but the zone will be otherwise empty.\n\
        # Configuring \"dnssec-lookaside auto;\" to activate this key is\n\
        # harmless, but is no longer useful and is not recommended.\n\
        dlv.isc.org. initial-key 257 3 5 \"BEAAAAPHMu/5onzrEE7z1egmhg/WPO0+juoZrW3euWEn4MxDCE1+lLy2\n\
                brhQv5rN32RKtMzX6Mj70jdzeND4XknW58dnJNPCxn8+jAGl2FZLK8t+\n\
                1uq4W+nnA3qO2+DL+k6BD4mewMLbIYFwe0PG73Te9fZ2kJb56dhgMde5\n\
                ymX4BI/oQ+cAK50/xvJv00Frf8kw6ucMTwFlgPe+jnGxPPEmHAte/URk\n\
                Y62ZfkLoBAADLHQ9IrS2tryAe7mbBZVcOwIeU/Rw/mRx/vwwMCTgNboM\n\
                QKtUdvNXDrYJDSHZws3xiRXF1Rf+al9UmZfSav/4NWLKjHzpT59k/VSt\n\
                TDN0YUuWrBNh\";\n\
\n\
        # ROOT KEYS: See https://data.iana.org/root-anchors/root-anchors.xml\n\
        # for current trust anchor information.\n\
        #\n\
        # These keys are activated by setting \"dnssec-validation auto;\"\n\
        # in named.conf.\n\
        #\n\
        # This key (19036) is to be phased out starting in 2017. It will\n\
        # remain in the root zone for some time after its successor key\n\
        # has been added. It will remain this file until it is removed from\n\
        # the root zone.\n\
        . initial-key 257 3 8 \"AwEAAagAIKlVZrpC6Ia7gEzahOR+9W29euxhJhVVLOyQbSEW0O8gcCjF\n\
                FVQUTf6v58fLjwBd0YI0EzrAcQqBGCzh/RStIoO8g0NfnfL2MTJRkxoX\n\
                bfDaUeVPQuYEhg37NZWAJQ9VnMVDxP/VHL496M/QZxkjf5/Efucp2gaD\n\
                X6RS6CXpoY68LsvPVjR0ZSwzz1apAzvN9dlzEheX7ICJBBtuA6G3LQpz\n\
                W5hOA2hzCTMjJPJ8LbqF6dsV6DoBQzgul0sGIcGOYl7OyQdXfZ57relS\n\
                Qageu+ipAdTTJ25AsRTAoub8ONGcLmqrAmRLKBP1dfwhYB4N7knNnulq\n\
                QxA+Uk1ihz0=\";\n\
\n\
        # This key (20326) is to be published in the root zone in 2017.\n\
        # Servers which were already using the old key (19036) should\n\
        # roll seamlessly to this new one via RFC 5011 rollover. Servers\n\
        # being set up for the first time can use the contents of this\n\
        # file as initializing keys; thereafter, the keys in the\n\
        # managed key database will be trusted and maintained\n\
        # automatically.\n\
        . initial-key 257 3 8 \"AwEAAaz/tAm8yTn4Mfeh5eyI96WSVexTBAvkMgJzkKTOiW1vkIbzxeF3\n\
                +/4RgWOq7HrxRixHlFlExOLAJr5emLvN7SWXgnLh4+B5xQlNVz8Og8kv\n\
                ArMtNROxVQuCaSnIDdD5LKyWbRd2n9WGe2R8PzgCmr3EgVLrjyBxWezF\n\
                0jLHwVN8efS3rCj/EWgvIWgb9tarpVUDK/b58Da+sqqls3eNbuv7pr+e\n\
                oZG+SrDK6nWeL3c6H5Apxz7LjVc1uTIdsIXxuOLYA4/ilBmSVIzuDWfd\n\
                RUfhHdY6+cn8HFRm+2hM8AnXGXws9555KrUB5qihylGa8subX2Nn6UwN\n\
                R1AkUTV74bU=\";\n\
};\n\
"
