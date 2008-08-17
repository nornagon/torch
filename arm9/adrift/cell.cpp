#include "cell.h"
#include "object.h"

#include "entities/terrain.h"

void Cell::reset() {
	type = TERRAIN_NONE;
	while (objects.top())
		delete objects.pop();
	creature = NULL;
}
