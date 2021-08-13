/* See LICENSE file for copyright and license details. */
#ifndef LIBPARSEPSF_H
#define LIBPARSEPSF_H

#include <stdint.h>
#include <stddef.h>


struct libparsepsf_unimap {
	struct libparsepsf_unimap *nonterminal[256];
	size_t terminal[256]; /* index + 1, 0 if not used */
};

struct libparsepsf_font {
	size_t num_glyphs;
	size_t height;
	size_t width;
	uint8_t *glyph_data;
	struct libparsepsf_unimap *map;
};


void libparsepsf_destroy_font(struct libparsepsf_font *font);
int libparsepsf_parse_font(const void *data, size_t size, struct libparsepsf_font *fontp, uint32_t *unrecognised_versionp);
size_t libparsepsf_get_glyph(const struct libparsepsf_font *font, const char *c, size_t *remp, const char **next_cp);

#endif
