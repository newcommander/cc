#include <string.h>
#include <stdio.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "font.h"

#define CHAR_NUM 94

static FT_Library libs[CHAR_NUM];
static FT_Face faces[CHAR_NUM];
static struct char_info cis[CHAR_NUM];

void get_char_info(char c, struct char_info **ci)
{
    if ((c < '!') || (c > '~'))
        *ci = NULL;
    else
        *ci = &cis[c - '!'];
}

int font_init(char *font_file)
{
    char check_chars[CHAR_NUM + 1];
    int ret = 0, i, j;

    memset(check_chars, 0, CHAR_NUM + 1);

    for (i = 0; i < CHAR_NUM; i++) {
        ret = FT_Init_FreeType(&libs[i]);
        if (ret) {
            printf("Init lib for '%c' failed, ret=0x%02X.\n", '!' + i, ret);
            while (i--)
                FT_Done_FreeType(libs[i]);
            return -1;
        }
    }

    for (i = 0; i < CHAR_NUM; i++) {
        ret = FT_New_Face(libs[i], font_file, 0, &faces[i]);
        if (ret) {
            printf("Create face for '%c' failed, ret=0x%02X.\n", '!' + i, ret);
            while (i--)
                FT_Done_Face(faces[i]);
            for (i = 0; i < CHAR_NUM; i++)
                FT_Done_FreeType(libs[i]);
            return -1;
        }
    }

    j = 0;
    for (i = 0; i < CHAR_NUM; i++) {
        ret = FT_Get_Char_Index(faces[i], '!' + i);
        if (!ret)
            check_chars[j++] = '!' + i;
    }

    if (j) {
        printf("Cannot find chars in font file: %s\n", check_chars);
        for (i = 0; i < CHAR_NUM; i++) {
            FT_Done_Face(faces[i]);
            FT_Done_FreeType(libs[i]);
        }
        return -1;
    }

    for (i = 0; i < CHAR_NUM; i++) {
        ret = FT_Set_Char_Size(faces[i], 0, 16*64, 100, 100);
        if (ret) {
            printf("Set char size for '%c' failed, ret=0x%02X.\n", '!' + i, ret);
            for (i = 0; i < CHAR_NUM; i++) {
                FT_Done_Face(faces[i]);
                FT_Done_FreeType(libs[i]);
            }
            return -1;
        }
    }

    for (i = 0; i < CHAR_NUM; i++) {
        ret = FT_Load_Char(faces[i], '!' + i, FT_LOAD_RENDER);
        if (ret) {
            printf("Load char '%c' failed, ret=0x%02X\n", '!' + i, ret);
            for (i = 0; i < CHAR_NUM; i++) {
                FT_Done_Face(faces[i]);
                FT_Done_FreeType(libs[i]);
            }
            return -1;
        }
    }

    memset(cis, 0, sizeof(struct char_info) * CHAR_NUM);
    for (i = 0; i < CHAR_NUM; i++) {
        cis[i].map_left = faces[i]->glyph->bitmap_left;
        cis[i].map_top = faces[i]->glyph->bitmap_top;
        cis[i].height = faces[i]->glyph->bitmap.rows;
        cis[i].width = faces[i]->glyph->bitmap.width;
        cis[i].map = faces[i]->glyph->bitmap.buffer;
        cis[i].advance = faces[i]->glyph->advance.x >> 6;
    }

    return 0;
}

void font_deinit()
{
    int i;

    for (i = 0; i < CHAR_NUM; i++) {
        FT_Done_Face(faces[i]);
        FT_Done_FreeType(libs[i]);
    }
}
