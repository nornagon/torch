#ifndef NATIVE_H
#define NATIVE_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <nds/jtypes.h>
#include <nds/arm9/video.h>

#define iprintf printf

enum {
	KEY_A,
	KEY_B,
	KEY_X,
	KEY_Y,
	KEY_L,
	KEY_R,
	KEY_RIGHT,
	KEY_LEFT,
	KEY_DOWN,
	KEY_UP,
	KEY_TOUCH,
};

enum IRQ_MASKS {
	IRQ_VBLANK = 0
};

typedef enum IRQ_MASKS IRQ_MASK;

void native_init();

void scanKeys();
uint32 keysHeld();
uint32 keysDown();

touchPosition touchReadXY();

void lcdMainOnBottom();
void lcdMainOnTop();

void swiWaitForVBlank();

int32 sqrtf32(int32 x);
int32 divf32(int32 num, int32 den);
void native_div_32_32_raw(int32 num, int32 den);

void drawcq(u32 x, u32 y, u32 c, u32 color);
void swapbufs();

extern vint32 DIV_RESULT32;
extern vuint16 DIV_CR;
#define DIV_BUSY (1<<15)

void irqInit();
void irqSet(IRQ_MASK irq, VoidFunctionPointer handler);
void irqEnable(IRQ_MASK irq);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_H */
