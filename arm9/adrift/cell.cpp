#include "cell.h"
#include "object.h"

#include "entities/terrain.h"

void Cell::reset() {
	type = TERRAIN_NONE;
	objects.delete_all();
	creature = NULL;
}
