#include "arrow.h"
/* This function appears in wsnap.c */
void
rescon (const int fd)
{
const struct	linger	die = {.l_onoff = 1, .l_linger = 0};

	setsockopt(fd, SOL_SOCKET, SO_LINGER, &die, sizeof(die));
	close(fd);
	/* No error handling on purpose */
}
