#ifndef FONT_H
#define FONT_H

struct char_info {
	int map_left;
	int map_top;
	unsigned int height;
	unsigned int width;
	unsigned char *map;
	long advance;
};

extern void get_char_info(char c, struct char_info **ci);
extern int font_init(char *font_file);
extern void font_deinit();

#endif /* FONT_H */
