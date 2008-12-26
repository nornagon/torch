#ifndef ENGINE_H
#define ENGINE_H 1

#ifdef NATIVE
#include <SDL.h>
#endif

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

		// scrolls the viewport (possibly) such that the point (x,y) is onscreen,
		// with at least margin cells between the point and the edge of the screen.
		void onscreen(int x, int y, unsigned int margin);

		// mark the entire screen for redrawing. Slow!
		void dirty_screen();

		// reset the luminance window (readjust your eyes)
		void reset_luminance();

		// draw the buffer
		void draw();

		void run(void (*handler)());

#ifdef NATIVE
		SDL_Surface *screen;
		SDL_Surface *backbuf;
#endif
};

extern engine torch;

#endif /* ENGINE_H */
