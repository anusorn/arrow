#include "arrow.h"
/* Looks up needle (nd) in haystack (hs), returns Streaming Point ID or 0 if not found */
int
rlookup(struct rpack_t **hs, const struct sockaddr *nd)
{
	int	i;
	uint8_t	*ad;
struct	rpack_t	const *rp;

	if (hs == NULL || nd == NULL) return 0;
	switch (nd->sa_family) {
	case AF_INET :
		ad = (uint8_t*) &((struct sockaddr_in*) nd)->sin_addr;
	mapped:	for (i = 0; i < 4; i++) if (hs[i] != NULL) {
			rp = &hs[i][ad[i]];
			if (rp->spid == 0) continue;
			do if (memcmp(ad, rp->iplow, i + 1) == 0)
				if (memcmp(ad + i + 1, rp->iplow + i + 1, 3 - i) >= 0\
				&& memcmp(rp->iphigh + i + 1, ad + i + 1, 3 - i) >= 0)
				return rp->spid;
			while ( (rp = rp->next) != NULL);
		}
		break;
	case AF_INET6 :
		ad = (uint8_t*) &((struct sockaddr_in6*) nd)->sin6_addr;
		if (IN6_IS_ADDR_V4MAPPED(ad) != 0) {
			ad += 12;
			goto mapped;
		}
		for (i = 0; i < 16; i++) if (hs[i] != NULL) {
			rp = &hs[i][ad[i]];
			if (rp->spid == 0) continue;
			do if (memcmp(ad, rp->iplow, i + 1) == 0)
				if (memcmp(ad + i + 1, rp->iplow + i + 1, 15 - i) >= 0\
				&& memcmp(rp->iphigh + i + 1, ad + i + 1, 15 - i) >= 0)
				return rp->spid;
			while ( (rp = rp->next) != NULL);
		}
		break;
	default :
		syslog(LOG_ERR, "rlookup Address Family: %d", nd->sa_family);
	}
	return 0;
}
