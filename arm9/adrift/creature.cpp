#include "creature.h"
#include <nds/arm9/video.h>

#ifdef ONLY_CNAMES
#define MON(def,ch,col,name) C_##def
#else
#define MON(def,ch,col,name) { ch, col, name }
#endif

#ifdef ONLY_CNAMES
enum {
#else
CreatureDesc creaturedesc[] = {
#endif
	MON(NONE,    'X', RGB15(31,0,0),   "NONE"),
	MON(PLAYER,  '@', RGB15(31,31,31), "player"),
	MON(FLYTRAP, 'V', RGB15(12,29,5),  "venus fly trap"),
};

#undef MON
