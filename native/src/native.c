#include "native.h"
#include <math.h>

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

void swiWaitForVBlank() {}

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
