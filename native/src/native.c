#include "native.h"
#include <math.h>
#include <SDL.h>

vuint32 REG_IE = 0;
VoidFunctionPointer __irq_vector[32];

vuint16 DIV_CR = 0;
vint32 DIV_RESULT32 = 0;

SDL_Surface *screen;

Uint32 vblank(Uint32 interval, void *param) {
	SDL_Flip(screen);
	return interval;
}

void native_init() {
	SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER);
	screen = SDL_SetVideoMode(256,192*2,16,SDL_SWSURFACE);
	SDL_AddTimer(17, vblank, NULL);
}

touchPosition touchReadXY() {
	touchPosition ret;
	ret.x = ret.y = ret.px = ret.py = ret.z1 = ret.z2 = 0;
	return ret;
}

void scanKeys() {}
uint32 keysHeld() { return 0; }
uint32 keysDown() { return 0; }

void lcdMainOnBottom() {}
void lcdMainOnTop() {}

void swiWaitForVBlank() {
	SDL_Delay(17);
	if (REG_IE & BIT(IRQ_VBLANK)) __irq_vector[IRQ_VBLANK]();
}

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
	putpixel(screen, x, y, rgb32from15(color));
}
void swapbufs() {
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
