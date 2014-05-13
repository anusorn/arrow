#include "arrow.h"
/* This function appears in sload.c */
void
freesdb(struct spdb_t *db)
{
	int	i;

	if (db != NULL) {
		if (db->set != NULL) free(db->set);
		if (db->lib != NULL) {
			for (i = 0; i < db->spmax; i++) if (db->lib[i] != NULL) free(db->lib[i]);
			free(db->lib);
		}
		if (db->ifnum != NULL) free(db->ifnum);
		free(db);
	}
}
