#include "arrow.h"

int
main(int argc, char **argv)
{
	FILE	*f;
const   int     on = 1;
	int	n, cpn, nth;
	socklen_t	len;
	pthread_t	tid;
	sigset_t	set;
struct	addrinfo	hints, *res, *reskeep;
struct	rpack_t		**rnew, **rold;
struct	spdb_t		*snew, *sold;
struct  sigaction       disp;

	openlog(argv[0], LOG_CONS | LOG_NDELAY | LOG_PERROR, LOG_LOCAL0);

	if (argc < 3) {
		fprintf(stderr, "\nUsage: %s <Listening_IP> <default_host_to_redirect_to>\n\n", argv[0]);
		_exit(1);
	}

	daemonize();

	defhost = argv[2];
	defholen = strlen(defhost);
	if ( (errno = pthread_rwlock_init(&cfglock, NULL)) != 0) {
		syslog(LOG_CRIT, "rwlock_init_main: %m");
		_exit(1);
	}
// Signal configuration for seamless config updates
	bzero(&disp, sizeof(disp));
	disp.sa_handler = SIG_IGN;
	if (sigaction(SIGHUP, &disp, NULL) < 0) {
		syslog(LOG_CRIT, "sigaction_main: %m");
		_exit(1);
	}
	sigemptyset(&set);
	sigaddset(&set, SIGHUP);
	if ( (errno = pthread_sigmask(SIG_BLOCK, &set, NULL)) != 0) {
		syslog(LOG_CRIT, "sigmask_main: %m");
		_exit(1);
	}
// Confirming CPU cores
	cpn = sysconf(_SC_NPROCESSORS_ONLN);	//may not be standard(?)
	if (cpn < 2) syslog(LOG_ERR, "_SC_NPROCESSORS_ONLN couldn't be retrieved, using: %d", (cpn = CPUNUM));
	else syslog(LOG_INFO, "available CPU cores: %d", cpn);
// Loading SPFILE
	if ( (spil = sload()) == NULL) {
		syslog(LOG_CRIT, "streaming points could not be loaded: %s", SPFILE);
		_exit(1);
	}
// WEB server section
	if ( (wcut = rload(WRANGE)) != NULL) {
	/* Opening up listening socket: IP from command line + PRTWEB */
		bzero(&hints, sizeof(hints));
		hints.ai_flags = AI_PASSIVE;
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		if ( (n = getaddrinfo(argv[1], PRTWEB, &hints, &res)) != 0) {
			syslog(LOG_CRIT, "getaddrinfo_web_main: %s", gai_strerror(n));
			_exit(1);
		}
		reskeep = res;
		n = BUFWEB;
		do {
			if ( (lifd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0) continue;
			if (setsockopt(lifd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) continue;
			if (setsockopt(lifd, SOL_SOCKET, SO_RCVBUF, &n, sizeof(n)) < 0) continue;
			if (setsockopt(lifd, SOL_SOCKET, SO_SNDBUF, &n, sizeof(n)) < 0) continue;
			/* LISTENQ is provided in /proc/sys/net/core/somaxconn (default 128) */
			if (bind(lifd, res->ai_addr, res->ai_addrlen) == 0 && listen(lifd, 2048) == 0) break;
			close(lifd);
		} while ( (res = res->ai_next) != NULL);
		if (res == NULL) {
			syslog(LOG_CRIT, "TCP_socket_creation_main: %m");
			_exit(1);
		}
		freeaddrinfo(reskeep);
		if ( (n = fcntl(lifd, F_GETFL, 0)) < 0 || fcntl(lifd, F_SETFL, n | O_NONBLOCK) < 0) {
			syslog(LOG_CRIT, "fcntl_main: %m");
			_exit(1);
		}
	/* Confiming number of concurrent connections */
		if ( (f = fopen(EPOLLL, "r")) == NULL) {
			syslog(LOG_CRIT, "fopen %s: %m", EPOLLL);
			_exit(1);
		}
		if (fscanf(f, "%d", &maxfd) != 1) {
			syslog(LOG_CRIT, "fscanf %s: %m", EPOLLL);
			_exit(1);
		}
		if ( (cots = calloc(maxfd + 1, sizeof(time_t))) == NULL) {
			syslog(LOG_CRIT, "calloc_cots_main: %m");
			_exit(1);
		}
		syslog(LOG_INFO, "Concurrent TCP connections: %d", maxfd);
		fclose(f);
		if ( (f = fopen(OFDMAX, "r")) == NULL) {
			syslog(LOG_CRIT, "fopen %s: %m", OFDMAX);
			_exit(1);
		}
		if (fscanf(f, "%d", &n) != 1) {
			syslog(LOG_CRIT, "fscanf %s: %m", OFDMAX);
			_exit(1);
		}
		if (n < maxfd + 1) syslog(LOG_ERR, "Warning: system wide OFDMAX: %d", n);
		fclose(f);
	/* Handling pthreads' start */
		nth = cpn - 1;	//CPU cores - 1 = number of WEB threads to start
		if ( (ring = calloc(nth * 2, sizeof(int))) == NULL) {
			syslog(LOG_CRIT, "calloc_ring_main: %m");
			_exit(1);
		}
		if (socketpair(AF_LOCAL, SOCK_STREAM, 0, ring) < 0) {
			syslog(LOG_CRIT, "socketpair_1_main: %m");
			_exit(1);
		}
		ring[nth * 2 - 1] = ring[1];	//signalling loop for Accept
		for (n = 1; n < nth; n++) if (socketpair(AF_LOCAL, SOCK_STREAM, 0, &ring[n * 2 - 1]) < 0) {
			syslog(LOG_CRIT, "socketpair___main: %m");
			_exit(1);
		}
		for (n = 0; n < nth; n++) if ( (errno = pthread_create(&tid, NULL, wsnap, &ring[n * 2])) != 0) {
			syslog(LOG_CRIT, "pthread_create_main_wsnap: %m");
			_exit(1);
		}
		if ( (errno = pthread_create(&tid, NULL, tidog, NULL)) != 0) {
			syslog(LOG_CRIT, "pthread_create_main_tidog: %m");
			_exit(1);
		}
		syslog(LOG_INFO, "WEB threads created");
		while (send(ring[nth * 2 - 1], &n, 1, 0) < 0) if (errno != EINTR) {
			syslog(LOG_CRIT, "failure to prime: %m");
			_exit(1);
		}
	}
// DNS server section
	if ( (dcut = rload(DRANGE)) != NULL) {
		if ( (errno = pthread_mutex_init(&udpserv, NULL)) != 0) {
			syslog(LOG_CRIT, "mutex_init_main: %m");
			_exit(1);
		}
	/* Opening up listening socket: IP from command line + PRTDNS */
		bzero(&hints, sizeof(hints));
		hints.ai_flags = AI_PASSIVE;
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;
		if ( (n = getaddrinfo(argv[1], PRTDNS, &hints, &res)) != 0) {
			syslog(LOG_CRIT, "getaddrinfo_dns_main: %s", gai_strerror(n));
			_exit(1);
		}
		reskeep = res;
		n = BUFDNS;
		do {
			if ( (dnsfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0) continue;
			if (setsockopt(dnsfd, SOL_SOCKET, SO_RCVBUF, &n, sizeof(n)) < 0) continue;
			if (bind(dnsfd, res->ai_addr, res->ai_addrlen) == 0) break;
			close(dnsfd);
		} while ( (res = res->ai_next) != NULL);
		if (res == NULL) {
			syslog(LOG_CRIT, "UDP_socket_creation_main: %m");
			_exit(1);
		}
		freeaddrinfo(reskeep);
	/* Confirming UDP socker SO_RCVBUF */
		len = sizeof(int);
		if (getsockopt(dnsfd, SOL_SOCKET, SO_RCVBUF, &n, &len) < 0) syslog(LOG_ERR, "UDP SO_RCVBUF unavailable: %m");
		else syslog(LOG_INFO, "UDP SO_RCVBUF set to %d", n);
	/* Handling threads */
		for (n = 0; n < cpn; n++) if ( (errno = pthread_create(&tid, NULL, dsnap, NULL)) != 0) {
			syslog(LOG_CRIT, "pthread_create_main_dsnap: %m");
			_exit(1);
		}
		syslog(LOG_INFO, "DNS threads created");
	}
	if (wcut == NULL && dcut == NULL) {
		syslog(LOG_CRIT, "No server to run: exiting");
		_exit(1);
	} else syslog(LOG_INFO, "system armed");
// Waiting for a signal to do errands
	for (; ;) {
		if ( (errno = sigwait(&set, &n)) != 0) {
			syslog(LOG_ERR, "sigwait_main: %m");
			sleep(9);
			continue;
		}
		if (n == SIGHUP) {
			if (wcut != NULL) {
				if ( (rnew = rload(WRANGE)) != NULL) {
					rold = wcut;
					pthread_rwlock_wrlock(&cfglock);
					wcut = rnew;
					pthread_rwlock_unlock(&cfglock);
					freecut(rold);
					syslog(LOG_INFO, "%s reloaded", WRANGE);
				} else syslog(LOG_ERR, "%s failed to reload", WRANGE);
			}
			if (dcut != NULL) {
				if ( (rnew = rload(DRANGE)) != NULL) {
					rold = dcut;
					pthread_rwlock_wrlock(&cfglock);
					dcut = rnew;
					pthread_rwlock_unlock(&cfglock);
					freecut(rold);
					syslog(LOG_INFO, "%s reloaded", DRANGE);
				} else syslog(LOG_ERR, "%s failed to reload", DRANGE);
			}
			if ( (snew = sload()) != NULL) {
				sold = spil;
				pthread_rwlock_wrlock(&cfglock);
				spil = snew;
				pthread_rwlock_unlock(&cfglock);
				freesdb(sold);
				syslog(LOG_INFO, "%s reloaded", SPFILE);
			} else syslog(LOG_ERR, "%s failed to reload", SPFILE);
		}
	}
}
