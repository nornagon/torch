#ifndef TEXT_H
#define TEXT_H 1

void text_init();
void text_display_clear();
void text_console_disable();
void text_console_enable();
void text_console_clear();
void text_console_rerender();
void text_console_render(const char *text);

void text_render_raw(int xoffset, int yoffset, const char *text, int textlen, unsigned short fgcolor);

unsigned char widthof(int c);

#endif /* TEXT_H */
