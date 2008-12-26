#include "native.h"
#include <math.h>
#include <SDL.h>

vuint32 REG_IE = 0;
VoidFunctionPointer __irq_vector[32];

vuint16 DIV_CR = 0;
vint32 DIV_RESULT32 = 0;

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

void drawcq(u32 x, u32 y, u32 c, u32 color) {
}
void swapbufs() {
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
