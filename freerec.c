#include "arrow.h"
/* This function appears in freecut.c */
void
freerec(struct rpack_t *ptr)
{
	if (ptr != NULL) {
		freerec(ptr->next);
		free(ptr);
	}
}
