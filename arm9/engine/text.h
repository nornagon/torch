#ifndef TEXT_H
#define TEXT_H 1

#include <nds/jtypes.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

void text_init();
void text_display_clear();
void text_console_disable();
void text_console_enable();
void text_console_clear();
void text_console_rerender();
void text_console_render(const char *text);

void text_render_raw(int xoffset, int yoffset, const char *text, int textlen, unsigned short fgcolor);
void tprintf(int x, int y, u16 color, const char *format, ...);

unsigned char widthof(int c);
int textwidth(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* TEXT_H */
