#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netns/ns.h>
#include <string.h>

int
net_addrcmp(sa1, sa2)
	struct sockaddr *sa1;
	struct sockaddr *sa2;
{
	if (sa1->sa_len != sa2->sa_len)
		return (sa1->sa_len < sa2->sa_len) ? -1 : 1;
	if (sa1->sa_family != sa2->sa_family)
		return (sa1->sa_family < sa2->sa_family) ? -1 : 1;

	switch(sa1->sa_family) {
	case AF_INET:
		return (memcmp(&((struct sockaddr_in *)sa1)->sin_addr,
		    &((struct sockaddr_in *)sa2)->sin_addr,
		    sizeof(struct in_addr)));
	case AF_INET6:
		return (memcmp(&((struct sockaddr_in6 *)sa1)->sin6_addr,
		    &((struct sockaddr_in6 *)sa2)->sin6_addr,
		    sizeof(struct in6_addr)));
	case AF_NS:
		return (memcmp(&((struct sockaddr_ns *)sa1)->sns_addr,
		    &((struct sockaddr_ns *)sa2)->sns_addr,
		    sizeof(struct ns_addr)));
	case AF_LOCAL:
		return (strcmp(((struct sockaddr_un *)sa1)->sun_path,
		    ((struct sockaddr_un *)sa1)->sun_path));
	default:
		return -1;
	}
}
