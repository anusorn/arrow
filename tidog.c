#include "arrow.h"
/* This is stale connections collector thread */
void*
tidog(void *arg)
{
		int		i;
volatile	int		d;
		time_t		now;
volatile	time_t		then;
const struct	sched_param	par = {.sched_priority = 0};

	if ( (errno = pthread_setschedparam(pthread_self(), SCHED_BATCH, &par)) != 0) {
		syslog(LOG_CRIT, "Setting collector's schedule: %m");
		_exit(1);
	}
	for (; ;) {
		sleep(1);
		time(&now);
		for (i = 0; i <= maxfd; i++) {
			then = cots[i];
			if (then != 0 && (d = now - then) < 0xFFFF && d > CTIMEO) {
				cots[i] = 0;
				rescon(i);
			}
		}
	}
}
