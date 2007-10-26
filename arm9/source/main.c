#include <nds.h>
#include <nds/arm9/console.h>

#include "engine.h"

#include "testworld.h"

int main(void) {
	torch_init();

	dirty_screen(); // the whole screen is dirty first frame.

	run(init_test());

	return 0;
}
