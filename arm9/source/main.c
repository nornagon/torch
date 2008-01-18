#include <nds.h>
#include <nds/arm9/console.h>

#include "engine.h"

#include "world.h"

int main(void) {
	torch_init();

	dirty_screen(); // the whole screen is dirty first frame.

	init_world();

	return 0;
}
