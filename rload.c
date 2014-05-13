#include "arrow.h"

struct rpack_t**
rload(const char *fname)
{
	FILE	*f;
	int	spid, i, cnt = 0;
	char	buf[1024], iplow[1024], iphigh[1024];
struct	rpack_t	**cut_local, *ptr, rp;
struct	sockaddr_in6	dstl, dsth;

	if ( (f = fopen(fname, "r")) == NULL) {
		syslog(LOG_ERR, "fopen %s: %m", fname);
		return NULL;
	}
	if ( (cut_local = calloc(16, sizeof(struct rpack_t*))) == NULL) {
		syslog(LOG_CRIT, "calloc_cut_rload: %m");
		fclose(f);
		return NULL;
	}
	while (fgets(buf, sizeof(buf), f) != NULL) {
		if (sscanf(buf, "%s %s %d", iplow, iphigh, &spid) != 3) {
			syslog(LOG_ERR, "%s line dropped: %s", fname, buf);
			continue;
		}
		if (spid == 0) {
			syslog(LOG_ERR, "%s: 0 Streaming Point ID dropped", fname);
			continue;
		}
		bzero(&rp, sizeof(rp));
		if (inet_pton(AF_INET6, iplow, &dstl) == 1) {
			if (inet_pton(AF_INET6, iphigh, &dsth) != 1) {
				syslog(LOG_ERR, "%s IPv6 conversion: %s", fname, iphigh);
				continue;
			} else {
				memcpy(rp.iplow, &dstl, 16);
				memcpy(rp.iphigh, &dsth, 16);
			}
		} else if (inet_pton(AF_INET, iplow, &dstl) == 1) {
			if (inet_pton(AF_INET, iphigh, &dsth) != 1) {
				syslog(LOG_ERR, "%s IPv4 conversion: %s", fname, iphigh);
				continue;
			} else {
				memcpy(rp.iplow, &dstl, 4);
				memcpy(rp.iphigh, &dsth, 4);
			}
		} else {
			syslog(LOG_ERR, "%s IP not recognized: %s", fname, iplow);
			continue;
		}
		rp.spid = spid;
		if (rp.iplow[0] != rp.iphigh[0]) {
			syslog(LOG_ERR, "%s: %s <-Most Significan Byte of a range must be fixed", fname, iplow);
			continue;
		}
		for (i = 1; i < 16; i++) if (rp.iplow[i] != rp.iphigh[i]) break;
		if (i == 16) {
			while (i > 0 && rp.iplow[i - 1] == 0) i--;
			if (i == 0) {
				syslog(LOG_ERR, "%s: zero address dropped", fname);
				continue;
			}
		}
		i--;	//here "i" is cut-off for the range and rp.iplow[i] is the hash byte for the range
		if (cut_local[i] == NULL) {	//here we have to create a hash-table for this cut-off
			if ( (cut_local[i] = calloc(256, sizeof(struct rpack_t))) == NULL) {
				syslog(LOG_CRIT, "calloc_hash_rload: %m");
				freecut(cut_local);
				fclose(f);
				return NULL;
			}
		}
		ptr = &cut_local[i][rp.iplow[i]];
		if (ptr->spid != 0) {	//hash collision
			while (ptr->next != NULL) ptr = ptr->next;
			if ( (ptr->next = calloc(1, sizeof(struct rpack_t))) == NULL) {
				syslog(LOG_CRIT, "calloc_list_rload: %m");
				freecut(cut_local);
				fclose(f);
				return NULL;
			}
			memcpy(ptr->next, &rp, sizeof(rp));
			cnt++;
		} else memcpy(ptr, &rp, sizeof(rp));
	}
	if (cnt > 1) syslog(LOG_WARNING, "%s: %d collisions", fname, cnt);
	fclose(f);
	return cut_local;
}
