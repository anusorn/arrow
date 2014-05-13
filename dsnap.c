#include "arrow.h"
/* DNS thread assumes dcut and spil are not NULL */
void*
dsnap(void *arg)
{
	int	n, i, j, k;
struct	timespec	tp;
	socklen_t	addrlen;
const	int	deflen = defholen + 4;
	char	buf[1024], def[deflen];
struct	sockaddr_storage	cliaddr;
	uint32_t	ttl = htonl(TTLDNS);
	uint16_t	val = htons(deflen - 2);
	char	cns[10] = {0xC0, 0xC, 0, 5, 0, 1};
	char	ais[12] = {0xC0, 0xC, 0, 1, 0, 1, 0, 0, 0, 0, 0, 4};
	char	a6s[12] = {0xC0, 0xC, 0, 28, 0, 1, 0, 0, 0, 0, 0, 16};

	memcpy(def, &val, 2);	//*(uint16_t*) def = htons(deflen - 2);
	memcpy(cns + 6, &ttl, 4);
	memcpy(ais + 6, &ttl, 4);
	memcpy(a6s + 6, &ttl, 4);
	strcpy(&def[3], defhost);
	n = i = deflen - 1;
	while (--i > 2) if (def[i] == '.') {
		def[i] = n - i - 1;
		n = i;
	}
	def[2] = n - 3;

	for (; ;) {
		addrlen = sizeof(cliaddr);
		if ( (errno = pthread_mutex_lock(&udpserv)) != 0) syslog(LOG_ERR, "mlock_dsnap: %m");
		while ( (n = recvfrom(dnsfd, buf, sizeof(buf), 0, (struct sockaddr*) &cliaddr, &addrlen)) < 0\
			&& errno == EINTR);
		if (n < 0) {
			syslog(LOG_CRIT, "recvfrom_dsnap: %m");
			_exit(1);
		}
		if ( (errno = pthread_mutex_unlock(&udpserv)) != 0) syslog(LOG_ERR, "ulock_dsnap: %m");
		/* DNS datagram is always up to 512 bytes in UDP, RFC 1035 */
		if (n < 20 || n > 512) {	//12 byte header, 4 bytes(QTYPE+QCLASS), 4 bytes min for domain name
			if ( (i = getnameinfo((struct sockaddr*) &cliaddr, addrlen,\
					buf, sizeof(buf), NULL, 0, NI_NUMERICHOST)) != 0) {
				syslog(LOG_ERR, "getnameinfo_query: %s", gai_strerror(i));
				continue;
			} else {
				syslog(LOG_ERR, "DNS query of %d bytes from: %s", n, buf);
				continue;
			}
		}
		if ((buf[2] & 0xF8) != 0) continue;	//we want Standard Query
		buf[2] = 0x84;	//we assemble Authoritative Response
		buf[3] = 0;	//No Recursion or DNSSEC
		if (buf[4] != 0 || buf[5] != 1) continue;	//we expect single Question
		memset(&buf[6], 0, 6);		//No Answer, Authority, and Additional sections
		for (i = 0xC; i < n && buf[i] != 0; i += buf[i] + 1);	//tracing domain name in Question
		if (i < n) n = i + 5;	//n-> Answer section
		else continue;	//bad format
/* here we have 4 options: no range (CNAME back), A, AAAA, Any */
		if ( (errno = pthread_rwlock_rdlock(&cfglock)) != 0) syslog(LOG_ERR, "cfglock_dsnap: %m");
		if ( (i = rlookup(dcut, (struct sockaddr*) &cliaddr)) == 0 || i > spil->spmax || spil->ifnum[--i] == 0) {
		//return CNAME
			if (i > spil->spmax) syslog(LOG_ERR, "%s inconsistent with %s", DRANGE, SPFILE);
			if ( (errno = pthread_rwlock_unlock(&cfglock)) != 0) syslog(LOG_ERR, "cfgunlock_dsnap: %m");
			buf[7]++;	//one Answer follows
			memcpy(&buf[n], cns, 10);
			n += 10;
			memcpy(&buf[n], def, deflen);
			n += deflen;
			if (n > 512) {
				if ( (i = getnameinfo((struct sockaddr*) &cliaddr, addrlen,\
						buf, sizeof(buf), NULL, 0, NI_NUMERICHOST)) != 0) {
					syslog(LOG_ERR, "getnameinfo_cname: %s", gai_strerror(i));
					continue;
				} else {
					syslog(LOG_ERR, "CNAME answer of %d bytes to: %s", n, buf);
					continue;
				}
			}
		} else {
			if (clock_gettime(CLOCK_MONOTONIC_RAW, &tp) < 0) {
				syslog(LOG_ERR, "clock_dsnap: %m");
				_exit(1);
			}
			j = (int) ((double) tp.tv_nsec / 1000000000 * spil->ifnum[i]);  //Random If
			if (buf[n - 3] == 1) {
			//A query. n->buf; i = SP index; j = interface of the SP
				for (k = 0; k < spil->ifnum[i]; k++) {
					if (n + 16 > 512) break;	//16 is the length of "A" answer
					if (inet_pton(AF_INET, spil->lib[i][j], &buf[n + 12]) == 1) {	//success
						buf[7]++;	//one more answer found
						memcpy(&buf[n], ais, 12);
						n += 16;
					}
					if (++j == spil->ifnum[i]) j = 0;	//wrap around
				}
			} else if (buf[n - 3] == 28) {
			//AAAA query
				for (k = 0; k < spil->ifnum[i]; k++) {
					if (n + 28 > 512) break;	//28 is the length of "AAAA" answer
					if (inet_pton(AF_INET6, spil->lib[i][j], &buf[n + 12]) == 1) {   //success
						buf[7]++;	//one more answer found
						memcpy(&buf[n], a6s, 12);
						n += 28;
					}
					if (++j == spil->ifnum[i]) j = 0;	//wrap around
				}
			} else if (buf[n - 3] == -1) {	//char is signed, so the max positive is 127
			//Any query
				for (k = 0; k < spil->ifnum[i]; k++) {
					if (inet_pton(AF_INET, spil->lib[i][j], &buf[n + 12]) == 1) {
						if (n + 16 > 512) break;
						memcpy(&buf[n], ais, 12);
						buf[7]++;
						n += 16;
					} else if (inet_pton(AF_INET6, spil->lib[i][j], &buf[n + 12]) == 1) {
						if (n + 28 > 512) break;
						memcpy(&buf[n], a6s, 12);
						buf[7]++;
						n += 28;
					}
					if (++j == spil->ifnum[i]) j = 0;
				}
			} else {	//bad request
				if ( (errno = pthread_rwlock_unlock(&cfglock)) != 0) syslog(LOG_ERR, "cfgunlock_dsnap: %m");
				continue;
			}
			if ( (errno = pthread_rwlock_unlock(&cfglock)) != 0) syslog(LOG_ERR, "cfgunlock_dsnap: %m");
		}
		while ( (i = sendto(dnsfd, buf, n, 0, (struct sockaddr*) &cliaddr, addrlen)) < 0 && errno == EINTR);
		if (i < 0) syslog(LOG_ERR, "sendto_dsnap: %m");
	}
}
