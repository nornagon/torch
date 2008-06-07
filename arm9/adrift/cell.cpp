#include "cell.h"
#include <nds/arm9/video.h>
#include "object.h"

CellDesc celldesc[] = {/* s  o  f */
	{ ' ', RGB15(31,31,31), 1, 0, 1 },
	{ '*', RGB15(4,31,1),   1, 1, 0 },
	{ '.', RGB15(17,9,6),   0, 0, 1 },
	{ '/', RGB15(4,12,30),  1, 0, 0 },
	{ '~', RGB15(5,14,23),  1, 0, 0 },
	{ 'w', RGB15(28,17,7),  1, 0, 0 },
};

void Cell::reset() {
	type = T_NONE;
	while (objects.top())
		delete objects.pop();
	creature = NULL;
}
