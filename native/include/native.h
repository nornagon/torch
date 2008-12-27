#ifndef NATIVE_H
#define NATIVE_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <nds/jtypes.h>
#include <nds/arm9/video.h>

#define iprintf printf

typedef enum KEYPAD_BITS {
  KEY_A      = BIT(0),
  KEY_B      = BIT(1),
  KEY_SELECT = BIT(2),
  KEY_START  = BIT(3),
  KEY_RIGHT  = BIT(4),
  KEY_LEFT   = BIT(5),
  KEY_UP     = BIT(6),
  KEY_DOWN   = BIT(7),
  KEY_R      = BIT(8),
  KEY_L      = BIT(9),
  KEY_X      = BIT(10),
  KEY_Y      = BIT(11),
  KEY_TOUCH  = BIT(12),
  KEY_LID    = BIT(13)
} KEYPAD_BITS;

enum IRQ_MASKS {
	IRQ_VBLANK = 0
};

typedef enum IRQ_MASKS IRQ_MASK;

#define DMA_ENABLE      BIT(31)
#define DMA_BUSY	    BIT(31)
#define DMA_IRQ_REQ     BIT(30)

#define DMA_START_NOW   0

#define DMA_16_BIT      0
#define DMA_32_BIT      BIT(26)

#define DMA_SRC_INC     (0)
#define DMA_SRC_DEC     BIT(23)

#define DMA_DST_INC     (0)
#define DMA_DST_DEC     BIT(21)

#define DMA_COPY_WORDS     (DMA_ENABLE | DMA_32_BIT | DMA_START_NOW)

#define DMA_SRC(n) dmaSrc[n]
#define DMA_DEST(n) dmaDest[n]
#define DMA_CR(n) dmaCR[n]
extern vuint32 dmaSrc[4];
extern vuint32 dmaDest[4];
extern vuint32 dmaCR[4];
void trip_dma(int);
bool dmaBusy(int);

extern u16 *backbuf;

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
