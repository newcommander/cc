#include <string.h>
#include <stdio.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#define CHAR_NUM 94

static FT_Library libs[CHAR_NUM];
static FT_Face faces[CHAR_NUM];

void draw_string(char *str)
{
	FT_GlyphSlot slot;
    int i, j, n, len;

    len = strlen(str);
	for (n = 0; n < len; n++) {
		if ((str[n] >= '!') && (str[n] <= '~')) {
			slot = faces[(int)(str[n] - '!')]->glyph;
			for (j = 0; j < slot->bitmap.rows; j++) {
				for (i = 0; i < slot->bitmap.width; i++)
					printf("%c", slot->bitmap.buffer[j * slot->bitmap.width + i] ? (slot->bitmap.buffer[j * slot->bitmap.width + i] > 128 ? '*' : '+') : ' ');
				printf("|\n");
			}
		} else if (str[n] == ' ') {
			printf("********************\n");
		} else {
			printf("--------------------\n");
		}
	}
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
		ret = FT_Set_Char_Size(faces[i], 0, 16*64, 300, 300);
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

	return 0;
}

void deinit_font()
{
    int i;

    for (i = 0; i < CHAR_NUM; i++) {
        FT_Done_Face(faces[i]);
        FT_Done_FreeType(libs[i]);
    }
}
