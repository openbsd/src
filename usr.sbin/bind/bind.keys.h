#ifndef BIND_KEYS_H
#define BIND_KEYS_H 1
#define TRUSTED_KEYS "\
# The bind.keys file is used to override the built-in DNSSEC trust anchors\n\
# which are included as part of BIND 9.  The only trust anchors it contains\n\
# are for the DNS root zone (\".\").  Trust anchors for any other zones MUST\n\
# be configured elsewhere; if they are configured here, they will not be\n\
# recognized or used by named.\n\
#\n\
# The built-in trust anchors are provided for convenience of configuration.\n\
# They are not activated within named.conf unless specifically switched on.\n\
# To use the built-in key, use \"dnssec-validation auto;\" in the\n\
# named.conf options.  Without this option being set, the keys in this\n\
# file are ignored.\n\
#\n\
# This file is NOT expected to be user-configured.\n\
#\n\
# These keys are current as of October 2017.  If any key fails to\n\
# initialize correctly, it may have expired.  In that event you should\n\
# replace this file with a current version.  The latest version of\n\
# bind.keys can always be obtained from ISC at https://www.isc.org/bind-keys.\n\
#\n\
# See https://data.iana.org/root-anchors/root-anchors.xml\n\
# for current trust anchor information for the root zone.\n\
\n\
trusted-keys {\n\
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
        # This key (20326) was published in the root zone in 2017.\n\
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
# which are included as part of BIND 9.  The only trust anchors it contains\n\
# are for the DNS root zone (\".\").  Trust anchors for any other zones MUST\n\
# be configured elsewhere; if they are configured here, they will not be\n\
# recognized or used by named.\n\
#\n\
# The built-in trust anchors are provided for convenience of configuration.\n\
# They are not activated within named.conf unless specifically switched on.\n\
# To use the built-in key, use \"dnssec-validation auto;\" in the\n\
# named.conf options.  Without this option being set, the keys in this\n\
# file are ignored.\n\
#\n\
# This file is NOT expected to be user-configured.\n\
#\n\
# These keys are current as of October 2017.  If any key fails to\n\
# initialize correctly, it may have expired.  In that event you should\n\
# replace this file with a current version.  The latest version of\n\
# bind.keys can always be obtained from ISC at https://www.isc.org/bind-keys.\n\
#\n\
# See https://data.iana.org/root-anchors/root-anchors.xml\n\
# for current trust anchor information for the root zone.\n\
\n\
managed-keys {\n\
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
        # This key (20326) was published in the root zone in 2017.\n\
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
#endif /* BIND_KEYS_H */
