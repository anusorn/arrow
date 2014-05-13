#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sched.h>
#include <linux/sched.h>
#include <signal.h>
#include <netdb.h>

/*
 * Format of the range files:
 * iplow(inclusive) iphigh(inclusive) Streaming_Point_ID(1-)
 */
#define WRANGE "range.web"
#define DRANGE "range.dns"
#define SPFILE "spids"	//Where a line is IPs of the SP
#define CPUNUM 8	//Number of CPU cores available
#define BUFWEB 4096
#define BUFDNS 512000	//"/proc/sys/net/ipv4/udp_mem"
#define CTIMEO 10	//Connection Time Out, seconds
#define PRTWEB "80"
#define PRTDNS "53"
#define TTLDNS 5	//86400 (1 day)
#define EPOLLL "/proc/sys/fs/epoll/max_user_watches"
#define OFDMAX "/proc/sys/fs/file-max"
#define REHEAD " 302 Found\r\nLocation: http://"

struct	rpack_t {	//Range Pack
unsigned	char	iphigh[16];
unsigned	char	iplow[16];
	int	spid;	//Streaming Point ID
struct	rpack_t	*next;
	}	**wcut, **dcut;	//*cutoffs[16];

struct	spdb_t	{	//SP Database
	int	spmax;	//Number of Streaming Points
	int	*ifnum;	//Number of Interfaces(IPs) per SP
	void	*set;
	char	***lib;	//ASCII content
	}	*spil;

	time_t	*cots;	//Connection time stamps
	int	*ring;	//Chain signalling for Accept
	int	lifd;	//Listening file descriptor
	int	maxfd;	//obtained off EPOLLL
	int	defholen;	//off argv[]
	char	*defhost;
	int	dnsfd;	//DNS socket

pthread_rwlock_t	cfglock;	// = PTHREAD_RWLOCK_INITIALIZER;
pthread_mutex_t		udpserv;	//To allow concurrent UDP server

struct rpack_t**	rload(const char*);	//Returns NULL if failed
void			freecut(struct rpack_t**);
void			freerec(struct rpack_t*);
struct spdb_t*		sload();		//Returns NULL if failed
void			freesdb(struct spdb_t*);
int			rlookup(struct rpack_t**, const struct sockaddr*);
void*			wsnap(void*);		//takes argument for *ring
void			rescon(const int);
void*			tidog(void*);		//Time watchdog
void*			dsnap(void*);		//DNS server thread
void			daemonize();
