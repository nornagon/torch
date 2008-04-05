#ifndef ENGINE_H
#define ENGINE_H 1

#include "buf.h"
#include "direction.h"
//#include "list.h"

class engine {
	private:
		s32 low_luminance;
		DIRECTION just_scrolled;
		int dirty;

		void move_port(DIRECTION);

	public:
		engine();

		torchbuf buf;
		void init();

		// scroll the screen one tile in the direction dir. This function moves the
		// "viewport" in the direction specified, via DMA. This function acts on
		// whatever is the current backbuffer.
		void scroll(int dsX, int dsY);

		// mark the entire screen for redrawing. Slow!
		void dirty_screen();

		// reset the luminance window (readjust your eyes)
		void reset_luminance();

		// draw the buffer
		void draw();

		void run(void (*handler)());
};

extern engine torch;

#endif /* ENGINE_H */
