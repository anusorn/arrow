#include "arrow.h"

void
daemonize()
{
	pid_t	pid;

	puts("\nDaemonizing...\n");
	if ( (pid = fork()) < 0) {
		syslog(LOG_ERR, "fork fail: %m\nrunning in shell");
		return;
	}
	if (pid != 0) exit(0);	//parent exits
	setsid();	//child becomes session leader
	if ( (pid = fork()) < 0) syslog(LOG_ERR, "fork fail: %m\nrunning as leader");
	else if (pid != 0) exit(0);
	if (fclose(stdin) != 0) syslog(LOG_ERR, "fclose_stdin_daemon: %m");
	if (fclose(stdout) != 0) syslog(LOG_ERR, "fclose_stdout_daemon: %m");
	if (fclose(stderr) != 0) syslog(LOG_ERR, "fclose_stderr_daemon: %m");
}
