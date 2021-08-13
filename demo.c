/* See LICENSE file for copyright and license details. */
#include "libparsepsf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


static size_t
readfile(int fd, char **datap, size_t *sizep)
{
	size_t len = 0;
	ssize_t r;
	for (;;) {
		if (len == *sizep) {
			*datap = realloc(*datap, *sizep += 4096);
			if (!*datap) {
				perror("realloc");
				exit(1);
			}
		}
		r = read(fd, &(*datap)[len], *sizep - len);
		if (r <= 0) {
			if (!r)
				break;
			perror("read");
			exit(1);
		}
		len += (size_t)r;
	}
	return len;
}


static uint8_t *
genreplacement(const struct libparsepsf_font *font)
{
	size_t glyph, size, i, linesize, xoff, yoff;
	int invert = 0, round = 0;
	uint8_t *data, xbit;

	glyph = libparsepsf_get_glyph(font, "�", NULL, NULL);
	if (!glyph) {
		invert = 1;
		round = 1;
		glyph = libparsepsf_get_glyph(font, "?", NULL, NULL);
		if (!glyph) {
			round = 0;
			glyph = libparsepsf_get_glyph(font, " ", NULL, NULL);
		}
	}
	glyph -= (glyph ? 1 : 0);

	linesize = font->width / 8 + (font->width % 8 ? 1 : 0);
	size = linesize * font->height;
	data = malloc(size);
	if (!data) {
		perror("malloc");
		exit(1);
	}

	memcpy(data, &font->glyph_data[glyph * size], size);
	if (invert)
		for (i = 0; i < size; i++)
			data[i] ^= 0xFF;
	if (round) {
		yoff = (font->height - 1) * linesize;
		xoff = (font->width - 1) / 8;
		xbit = 0x80 >> ((font->width - 1) % 8);
		data[0] ^= 0x80;
		data[yoff] ^= 0x80;
		data[xoff] ^= xbit;
		data[yoff + xoff] ^= xbit;
	}

	return data;
}


static void
printglyphrow(const struct libparsepsf_font *font, const uint8_t *gd, const char *bg, const char *fg)
{
	uint8_t bit;
	size_t x;

	bit = 0x80;
	for (x = 0; x < font->width; x++) {
		printf("%s", (*gd & bit) ? fg : bg);
		bit >>= 1;
		if (!bit) {
			bit = 0x80;
			gd = &gd[1];
		}
	}
}


static void
printglyph(const struct libparsepsf_font *font, size_t glyph, const char *bg, const char *fg)
{
	const uint8_t *gd;
	size_t width = font->width / 8 + (font->width % 8 ? 1 : 0);
	size_t glyphsize = width * font->height;
	size_t y;

	if (glyph > font->num_glyphs) {
		fprintf(stderr, "glyph does not exist!\n");
		exit(1);
	}

	for (y = 0; y < font->height; y++) {
		gd = &font->glyph_data[glyph * glyphsize + y * width];
		printglyphrow(font, gd, bg, fg);
		printf("\033[0m\n");
	}
}


static void
printline(const struct libparsepsf_font *font, const char *line, size_t linelen)
{
	const char *c, *bg, *fg;
	const uint8_t *gd;
	size_t width = font->width / 8 + (font->width % 8 ? 1 : 0);
	size_t glyphsize = width * font->height;
	size_t glyph, rem, y;
	uint8_t *replacement = genreplacement(font);

	for (y = 0; y < font->height; y++) {
		rem = linelen;
		for (c = line; rem;) {
			glyph = libparsepsf_get_glyph(font, c, &rem, &c);
			if (!glyph) {
				do {
					c = &c[1];
					rem -= 1;
				} while ((*c & 0xC0) == 0x80);
				bg = "\033[1;30;40m[]";
				fg = "\033[1;31;41m[]";
				gd = &replacement[y * width];
			} else {
				glyph -= 1;
				bg = "\033[1;30;40m[]";
				fg = "\033[1;37;47m[]";
				gd = &font->glyph_data[glyph * glyphsize + y * width];
			}
			printglyphrow(font, gd, bg, fg);
		}
		printf("\033[0m\n");
	}

	free(replacement);
}


int
main(int argc, char *argv[])
{
	unsigned long int glyph;
	uint32_t uver;
	char *data = NULL;
	size_t len, size = 0;
	struct libparsepsf_font font;

	if (argc) {
		argc--;
		argv++;
	}

	len = readfile(STDIN_FILENO, &data, &size);
	if (libparsepsf_parse_font(data, len, &font, &uver)) {
		perror("libparsepsf_parse_font");
		free(data);
		exit(1);
	}
	free(data);

	if (uver) {
		fprintf(stderr, "WARNING: Font format version is not fully supported: "
		                "version 2.%lu\n", (unsigned long int)uver);
	}

	printf("#glyphs = %zu\n", font.num_glyphs);
	printf("width   = %zu\n", font.width);
	printf("height  = %zu\n", font.height);

	if (!argc) {
		printline(&font, "", 1);
	} else if (argc == 2 && !strcmp(argv[0], "-g")) {
		glyph = strtoul(argv[1], NULL, 0);
		printglyph(&font, (size_t)glyph, "\033[1;30;40m[]", "\033[1;37;47m[]");
	} else if (argc == 2 && !strcmp(argv[0], "-x")) {
		glyph = strtoul(argv[1], NULL, 16);
		printglyph(&font, (size_t)glyph, "\033[1;30;40m[]", "\033[1;37;47m[]");
	} else {
		for (; argc--; argv++)
			printline(&font, *argv, strlen(*argv));
	}

	libparsepsf_destroy_font(&font);
	return 0;
}
