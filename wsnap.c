#include "arrow.h"
/* This is a thread handling HTTP Request Routing */
void*
wsnap(void *inon)
{
	int	const minev = 50;
	int	acev = minev, efd, nfds, i, j, f;
	char	buf[BUFWEB], suf[BUFWEB], *uri, *sep, *sptr;
	char	wd[4], mo[4], dn[3], tm[9], yr[5];
	time_t	now;
	socklen_t	solen;
struct	epoll_event	ev, *events, *tev;
struct	sockaddr_storage sas;
struct	timespec	tp;

	if ( (events = calloc(minev, sizeof(ev))) == NULL) {
		syslog(LOG_CRIT, "calloc_minev_wsnap: %m");
		_exit(1);
	}
	if ( (efd = epoll_create1(0)) < 0) {
		syslog(LOG_CRIT, "epoll_create_wsnap: %m");
		_exit(1);
	}
	bzero(&ev, sizeof(ev));
	ev.data.fd = lifd;
	if (epoll_ctl(efd, EPOLL_CTL_ADD, lifd, &ev) < 0) {
		syslog(LOG_CRIT, "epoll_add_lifd: %m");
		_exit(1);
	}
	ev.events = EPOLLIN;
	ev.data.fd = ((int*) inon)[0];
	//syslog(LOG_INFO, "ring fd: %d", ev.data.fd);
	if (epoll_ctl(efd, EPOLL_CTL_ADD, ev.data.fd, &ev) < 0) {
		syslog(LOG_CRIT, "epoll_ctl_ring: %m");
		_exit(1);
	}
	for (; ;) {
		if ( (nfds = epoll_wait(efd, events, acev, -1)) < 0) {
			if (errno == EINTR) continue;
			else {
				syslog(LOG_CRIT, "epoll_wait_wsnap: %m");
				_exit(1);
			}
		}
		for (i = 0; i < nfds; i++) {
			if (events[i].data.fd == ((int*) inon)[0]) {
				//syslog(LOG_INFO, "ring chain %d", ((int*) inon)[0]);
				recv(((int*) inon)[0], buf, sizeof(buf), MSG_DONTWAIT);
				ev.data.fd = lifd;	//listening file descriptor
				ev.events = EPOLLIN | EPOLLONESHOT;
				if (epoll_ctl(efd, EPOLL_CTL_MOD, lifd, &ev) < 0) {
					syslog(LOG_CRIT, "epoll_mod_lifd: %m");
					_exit(1);
				}
			} else if (events[i].data.fd == lifd) {
				//syslog(LOG_INFO, "accept chain %d", ((int*) inon)[0]);
				if ( (f = accept(lifd, NULL, NULL)) < 0) syslog(LOG_ERR, "%d accept: %m", ((int*) inon)[0]);
				while ( (j = send(((int*) inon)[1], buf, 1, MSG_DONTWAIT)) < 0 && errno == EINTR);
				if (j <= 0) {
					syslog(LOG_CRIT, "send_ring_wsnap 0 or: %m");
					_exit(1);
				}
				if (f < 0) continue;
				else if (f > maxfd) {
					syslog(LOG_ERR, "FD %d too large: connection dropped", f);
					rescon(f);
				} else {
					time(&cots[f]);
					if ( (j = fcntl(f, F_GETFL, 0)) < 0) {
						syslog(LOG_CRIT, "fcntl_get_wsnap: %m");
						_exit(1);
					}
					if (fcntl(f, F_SETFL, j | O_NONBLOCK) < 0) {
						syslog(LOG_CRIT, "fcntl_set_wsnap: %m");
						_exit(1);
					}
					ev.data.fd = f;
					ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
					if (epoll_ctl(efd, EPOLL_CTL_ADD, f, &ev) < 0) syslog(LOG_ERR, "epoll_add_con: %m");
				}
			} else if (events[i].events & EPOLLRDHUP) {
				/* Graceful shutdown */
				//syslog(LOG_INFO, "close chain %d", ((int*) inon)[0]);
				while (close(events[i].data.fd) < 0 && errno == EINTR);
				cots[events[i].data.fd] = 0;
			} else {
				/* Here we are expected to handle HTTP requests */
				//syslog(LOG_INFO, "request chain %d", ((int*) inon)[0]);
				while ( (j = recv(events[i].data.fd, buf, sizeof(buf), MSG_PEEK)) < 0 && errno == EINTR);
				if (j > 4 && memcmp(&buf[j - 4], "\r\n\r\n", 4) == 0) {	//valid HTTP request all in
					if (memcmp(buf, "GET ", 4) != 0 && memcmp(buf, "HEAD", 4) != 0) continue;
					buf[j] = '\0';	//HTTP request to string
					uri = strtok_r(buf + 4, " \r", &sptr);
					sep = strtok_r(NULL, " \r", &sptr);	//send buf <= HTTP/1.1
					if ( (j = sptr - sep - 1) == 0) continue;	//"HEAD\r\n\r\n" attack
					sep = memcpy(suf, sep, j) + j;
					memcpy(sep, REHEAD, sizeof(REHEAD) - 1);
					sep += sizeof(REHEAD) - 1;
					solen = sizeof(sas);
					if (getpeername(events[i].data.fd, (struct sockaddr*) &sas, &solen) < 0) {
						syslog(LOG_ERR, "getpeername: %m");
						continue;
					}
					if ( (errno = pthread_rwlock_rdlock(&cfglock)) != 0) syslog(LOG_ERR, "cfglock_wsnap: %m");
					j = rlookup(wcut, (struct sockaddr*) &sas);
					if (j == 0 || j > spil->spmax || spil->ifnum[--j] == 0) {
						if (j > spil->spmax) syslog(LOG_ERR, "%s inconsistent with %s", WRANGE, SPFILE);
						sep = memcpy(sep, defhost, defholen) + defholen;
					} else {	//Random Streaming Interface (IP)
						if (clock_gettime(CLOCK_MONOTONIC_RAW, &tp) < 0) {
							syslog(LOG_ERR, "clock_wsnap: %m");
							_exit(1);
						}
						f = (int) ((double) tp.tv_nsec / 1000000000 * spil->ifnum[j]);
						solen = strlen(spil->lib[j][f]);
						sep = memcpy(sep, spil->lib[j][f], solen) + solen;
					}
					if ( (errno = pthread_rwlock_unlock(&cfglock)) != 0) syslog(LOG_ERR, "cfgunlock_wsnap: %m");
					if (uri[0] == '/') {	//Looking for Host header
						if ( (sptr = strstr(sptr, "\nHost: ")) == NULL) continue;	//no Host header
						sptr += sizeof("\nHost: ") - 1;
						*sep++ = '/';
						if ( (j = strchr(sptr, '\r') - sptr) <= 0) continue;	//no Host header content
						sep = memcpy(sep, sptr, j) + j;
					} else {
						if (strncasecmp(uri, "http://", sizeof("http:/")) == 0) uri += sizeof("http:");
						else continue;
					}
					j = strlen(uri);
					sep = memcpy(sep, uri, j) + j;	//URL completed
					time(&now);
					sscanf(ctime_r(&now, buf), "%s %s %s %s %s", wd, mo, dn, tm, yr);
					solen = sep - suf + sprintf(sep,\
						"\r\nDate: %s, %s %s %s %s GMT\r\nConnection: Close\r\nContent-Length: 0\r\n\r\n",\
						wd, dn, mo, yr, tm);
					j = 0;
					while (j < solen) {
						if ( (f = send(events[i].data.fd, suf + j, solen - j, MSG_NOSIGNAL)) < 0) {
							if (errno == EINTR) continue;
							else {
								syslog(LOG_ERR, "send_data_wsnap: %m");
								break;
							}
						}
						if (f == 0) break;
						j += f;
					}
				}
			}
		}
		if (nfds == acev) {
			if ( (tev = calloc(acev * 2, sizeof(ev))) != NULL) {
				acev *= 2;
				free(events);
				events = tev;
			}
		} else if (nfds > minev && nfds * 2 < acev && (tev = calloc(acev / 2, sizeof(ev))) != NULL) {
			acev /= 2;
			free(events);
			events = tev;
		}
	}
}
