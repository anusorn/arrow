#include "arrow.h"

struct spdb_t*
sload()
{
	int	i, j;
	char	*set, buf[4096], tmp[4096], *sav;
	FILE	*f;
struct	spdb_t	*db;

	if ( (db = calloc(1, sizeof(struct spdb_t))) == NULL) {
		syslog(LOG_ERR, "calloc_spdb: %m");
		return NULL;
	}
	if ( (f = fopen(SPFILE, "r")) == NULL) {
		syslog(LOG_ERR, "fopen %s: %m", SPFILE);
		freesdb(db);
		return NULL;
	}
	while (fgets(buf, sizeof(buf), f) != NULL) db->spmax++;
	if (db->spmax > 0 && buf[0] == '\n') db->spmax--;	//account for closing NL
	if ( (db->ifnum = calloc(db->spmax, sizeof(int))) == NULL) {
		syslog(LOG_ERR, "calloc_ifnum: %m");
		freesdb(db); fclose(f); return NULL;
	}
	if ( (db->lib = calloc(db->spmax, sizeof(char**))) == NULL) {
		syslog(LOG_ERR, "calloc_splib1: %m");
		freesdb(db); fclose(f); return NULL;
	}
	if ( (i = (int) ftell(f)) < 0) {
		syslog(LOG_ERR, "ftell %s: %m", SPFILE);
		freesdb(db); fclose(f); return NULL;
	}
	if ( (db->set = set = malloc(i)) == NULL) {
		syslog(LOG_ERR, "malloc_set: %m");
		freesdb(db); fclose(f); return NULL;
	}
	rewind(f);	//pass #2:
	for (i = 0; fgets(buf, sizeof(buf), f) != NULL && i < db->spmax; i++) {
		if (buf[0] == '#') continue;	//Streaming Point out of service
		strcpy(tmp, buf);
		if (strtok_r(tmp, "\t \r\n", &sav) != NULL) {
			for (j = 1; strtok_r(NULL, "\t \r\n", &sav) != NULL; j++);
			db->ifnum[i] = j;
			if ( (db->lib[i] = calloc(j, sizeof(char*))) == NULL) {
				syslog(LOG_ERR, "calloc_splib2: %m");
				freesdb(db); fclose(f); return NULL;
			}
			db->lib[i][--j] = strcpy(set, strtok_r(buf, "\t \r\n", &sav));
			set += strlen(set) + 1;
			while (j > 0) {
				db->lib[i][--j] = strcpy(set, strtok_r(NULL, "\t \r\n", &sav));
				set += strlen(set) + 1;
			}
		}
	}
	fclose(f);
	return db;
}
