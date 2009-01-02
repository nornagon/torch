#include "native.h"
#include "font.h"
#include <math.h>
#include <SDL.h>
#include <string.h>

vuint32 REG_IE = 0;
VoidFunctionPointer __irq_vector[32];

vuint16 DIV_CR = 0;
vint32 DIV_RESULT32 = 0;
vuint32 dmaSrc[4];
vuint32 dmaDest[4];
vuint32 dmaCR[4];
u16 *backbuf = 0;

SDL_Surface *screen, *backbuffer, *frontbuffer;
SDL_cond *vblankCond;
SDL_mutex *vblankMutex;
SDL_mutex *keysMutex;

Uint32 vblank(Uint32 interval, void *param);

void native_init() {
	SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER);
	screen = SDL_SetVideoMode(256,192*2,16,SDL_SWSURFACE);
	SDL_PixelFormat *fmt = malloc(sizeof(SDL_PixelFormat));
	memcpy(fmt, screen->format, sizeof(SDL_PixelFormat));
	fmt->Rmask = 0x1f;
	fmt->Rshift = 0;
	fmt->Rloss = 3;
	fmt->Gmask = 0x3e0;
	fmt->Gshift = 5;
	fmt->Gloss = 3;
	fmt->Bmask = 0x7c00;
	fmt->Bshift = 10;
	fmt->Bloss = 3;
	fmt->Amask = 0;
	fmt->Ashift = 0;
	fmt->Aloss = 8;
	backbuffer = SDL_ConvertSurface(screen, fmt, SDL_SWSURFACE);
	frontbuffer = SDL_ConvertSurface(screen, fmt, SDL_SWSURFACE);
	free(fmt);
	backbuf = backbuffer->pixels;

	vblankMutex = SDL_CreateMutex();
	vblankCond = SDL_CreateCond();
	keysMutex = SDL_CreateMutex();

	int i;
	for (i = 0; i < 4; i++) {
		dmaSrc[i] = dmaDest[i] = dmaCR[i] = 0;
	}

	SDL_AddTimer(15, vblank, NULL);
}

touchPosition touchReadXY() {
	touchPosition ret;
	ret.x = ret.y = ret.px = ret.py = ret.z1 = ret.z2 = 0;
	return ret;
}

uint32 dsKeyForSDLKey(SDLKey key) {
	switch (key) {
		case 'a': return KEY_A;
		case 'b': return KEY_B;
		case 'x': return KEY_X;
		case 'y': return KEY_Y;
		case 'r': return KEY_R;
		case 'l': return KEY_L;
		case SDLK_LEFT: return KEY_LEFT;
		case SDLK_RIGHT: return KEY_RIGHT;
		case SDLK_UP: return KEY_UP;
		case SDLK_DOWN: return KEY_DOWN;
		case SDLK_RETURN: return KEY_START;
		case SDLK_RSHIFT: return KEY_SELECT;
		default: break;
	}

	return 0;
}

uint32 currentKeyState[2];
uint32 downKeyState[2];
void scanKeys() {
	SDL_LockMutex(keysMutex);
	currentKeyState[0] = currentKeyState[1];
	downKeyState[0] = downKeyState[1];
	downKeyState[1] = 0;
	SDL_UnlockMutex(keysMutex);
}

uint32 keysHeld() {
	return currentKeyState[0];
}

uint32 keysDown() {
	return downKeyState[0];
}

Uint32 vblank(Uint32 interval, void *param) {
	SDL_FillRect(screen, NULL, 0);
	SDL_BlitSurface(frontbuffer, NULL, screen, NULL);
	SDL_Flip(screen);
	SDL_CondSignal(vblankCond);
	SDL_LockMutex(keysMutex);
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_KEYDOWN:
				downKeyState[1] |= dsKeyForSDLKey(event.key.keysym.sym);
				currentKeyState[1] |= dsKeyForSDLKey(event.key.keysym.sym);
				break;
			case SDL_KEYUP:
				currentKeyState[1] &= ~dsKeyForSDLKey(event.key.keysym.sym);
				break;
			case SDL_QUIT:
				exit(0);
		}
	}
	SDL_UnlockMutex(keysMutex);
	return interval;
}

void swiWaitForVBlank() {
	SDL_mutexP(vblankMutex);
	SDL_CondWait(vblankCond, vblankMutex);
	SDL_mutexV(vblankMutex);
	if (REG_IE & BIT(IRQ_VBLANK)) __irq_vector[IRQ_VBLANK]();
}

void lcdMainOnBottom() {}
void lcdMainOnTop() {}

void putpixel(SDL_Surface *surface, int x, int y, Uint32 pixel)
{
    int bpp = surface->format->BytesPerPixel;
    /* Here p is the address to the pixel we want to set */
    Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;

    switch(bpp) {
    case 1:
        *p = pixel;
        break;

    case 2:
        *(Uint16 *)p = pixel;
        break;

    case 3:
        if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
            p[0] = (pixel >> 16) & 0xff;
            p[1] = (pixel >> 8) & 0xff;
            p[2] = pixel & 0xff;
        } else {
            p[0] = pixel & 0xff;
            p[1] = (pixel >> 8) & 0xff;
            p[2] = (pixel >> 16) & 0xff;
        }
        break;

    case 4:
        *(Uint32 *)p = pixel;
        break;
    }
}

Uint32 rgb32from15(u32 color) {
	u32 r = color & 0x1f,
	    g = (color >> 5) & 0x1f,
	    b = (color >> 10) & 0x1f;
	return (r<<11)|(g<<6)|(b<<1);
}

void drawcq(u32 x, u32 y, u32 c, u32 color) {
	c -= ' ';
	const unsigned char *chardata = &fontTiles[8 * 8 * c];

	unsigned int i, j;
	for (i = 0; i < 8; i++) {
		for (j = 0; j < 8; j++) {
			unsigned int offset = i + (j * 8);
			Uint32 col = 0;
			if (chardata[offset])
				col = color;
			putpixel(backbuffer, x + i, y + j, col);
		}
	}
}

void swapbufs() {
	SDL_Surface *tmp = backbuffer;
	backbuffer = frontbuffer;
	frontbuffer = tmp;
	backbuf = backbuffer->pixels;
}

void native_div_32_32_raw(int32 num, int32 den) {
	DIV_RESULT32 = num/den;
}

int32 sqrtf32(int32 x) {
	float a = x / 4096.0f;
	return sqrtf(a) * 4096.0f;
}

int32 divf32(int32 num, int32 den) {
	return 4096.0f * (float)num/(float)den;
}

void irqInit() {
	int i;
	for (i = 0; i < 32; i++) __irq_vector[i] = 0;
}

void irqSet(IRQ_MASK irq, VoidFunctionPointer f) {
	__irq_vector[irq] = f;
}

void irqEnable(IRQ_MASK irq) {
	REG_IE |= BIT(irq);
}

void trip_dma(int n) {
	if (dmaCR[n] & DMA_ENABLE) {
		int nwords = dmaCR[n] & 0x1fffff;
		int copied;
		for (copied = 0; copied < nwords; copied++) {
			*((vuint32*)dmaDest[n]) = *((vuint32*)dmaSrc[n]);
			if (dmaCR[n] & DMA_SRC_DEC) dmaSrc[n] -= 4;
			else dmaSrc[n] += 4;
			if (dmaCR[n] & DMA_DST_DEC) dmaDest[n] -= 4;
			else dmaDest[n] += 4;
		}
		dmaCR[n] = 0;
	}
}

bool dmaBusy(int n) {
	return dmaCR[n] & DMA_BUSY;
}
