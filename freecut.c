#include "arrow.h"
/* This function appears in rload.c */
void
freecut(struct rpack_t **cut)
{
	int	i, j;

	if (cut != NULL) {
		for (i = 0; i < 16; i++) {
			if (cut[i] != NULL) {
				for (j = 0; j < 256; j++) freerec(cut[i][j].next);
				free(cut[i]);
			}
		}
		free(cut);
	}
}
