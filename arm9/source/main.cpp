#include <nds.h>
#ifndef NATIVE
#include <fat.h>
#endif

#include "engine.h"
#include "world.h"

int main(void) {
	torch.init();

#ifndef NATIVE
	bool fatSucceeded = fatInitDefault();
	assert(fatSucceeded);
#endif

	torch.dirty_screen(); // the whole screen is dirty first frame.

	init_world();

	return 0;
}
