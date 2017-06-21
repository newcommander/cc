#ifndef FONT_H
#define FONT_H

struct char_info {
	int map_left;
	int mpa_top;
	unsigned int height;
	unsigned int width;
	unsigned char *map;
	long advance;
};

extern void draw_string(char *str);
extern int font_init(char *font_file);
extern void font_deinit();

#endif /* FONT_H */
