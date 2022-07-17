/* See LICENSE file for copyright and license details. */
#include "libparsepsf.h"

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <grapheme.h>


struct psf1_header {
	uint8_t magic_x36;
	uint8_t magic_x04;
	uint8_t mode;
#define PSF1_MODE512    0x01
#define PSF1_MODEHASTAB 0x02
/* #define PSF1_MODEHASSEQ 0x04 // really used */
	uint8_t height;
};
#define PSF1_SEPARATOR  0xFFFF
#define PSF1_STARTSEQ   0xFFFE

struct psf2_header {
	uint8_t magic_x72;
	uint8_t magic_xb5;
	uint8_t magic_x4a;
	uint8_t magic_x86;
	uint32_t version;
#define PSF2_MAXVERSION 0
	uint32_t header_size;
	uint32_t flags;
#define PSF2_HAS_UNICODE_TABLE 0x01
	uint32_t num_glyphs;
	uint32_t charsize; /* = height * ((width + 7) / 8) */
	uint32_t height;
	uint32_t width;
};
#define PSF2_SEPARATOR  0xFF
#define PSF2_STARTSEQ   0xFE


static void
free_map(struct libparsepsf_unimap *node)
{
	size_t i;
	for (i = 0; i < sizeof(node->nonterminal) / sizeof(*node->nonterminal); i++)
		if (node->nonterminal[i])
			free_map(node->nonterminal[i]);
	free(node);
}

void
libparsepsf_destroy_font(struct libparsepsf_font *font)
{
	free(font->glyph_data);
	font->glyph_data = NULL;
	if (font->map) {
		free_map(font->map);
		font->map = NULL;
	}
}


static uint16_t
letoh16(const uint8_t *le)
{
	uint16_t b0 = (uint16_t)((uint16_t)le[0] << 0);
	uint16_t b1 = (uint16_t)((uint16_t)le[1] << 8);
	return (uint16_t)(b0 | b1);
}


static uint32_t
letoh32(uint32_t le)
{
	union {
		uint32_t v;
		uint8_t b[4];
	} u = {.v = le};
	uint32_t b0 = (uint32_t)((uint32_t)u.b[0] << 0);
	uint32_t b1 = (uint32_t)((uint32_t)u.b[1] << 8);
	uint32_t b2 = (uint32_t)((uint32_t)u.b[2] << 16);
	uint32_t b3 = (uint32_t)((uint32_t)u.b[3] << 24);
	return (uint32_t)(b0 | b1 | b2 | b3);
}


static uint32_t
desurrogate(uint16_t high, uint16_t low)
{
	/* high surrogate has lower value */
	uint32_t h = UINT32_C(0xD800) ^ (uint32_t)high;
	uint32_t l = UINT32_C(0xDC00) ^ (uint32_t)low;
	h <<= 10;
	return (uint32_t)(h | l);
}

static int
put_map_incomplete(struct libparsepsf_font *font, const uint8_t *seq, size_t seqlen,
                   uint8_t *savedp, struct libparsepsf_unimap **nodep)
{
	size_t i;
	if (font->map == NULL) {
		font->map = calloc(1, sizeof(*font->map));
		if (!font->map)
			goto enomem;
	}
	if (!seqlen)
		goto ebfont;
	*nodep = font->map;
	*savedp = seq[--seqlen];
	for (i = 0; i < seqlen; i++) {
		if (!(*nodep)->nonterminal[seq[i]]) {
			(*nodep)->nonterminal[seq[i]] = calloc(1, sizeof(*font->map));
			if (!(*nodep)->nonterminal[seq[i]])
				goto enomem;
		}
		*nodep = (*nodep)->nonterminal[seq[i]];
	}

	return 0;

ebfont:
	errno = EBFONT;
	return -1;
enomem:
	errno = ENOMEM;
	return -1;
}

static int
put_map_finalise(size_t index, uint8_t saved, struct libparsepsf_unimap *node)
{
	/* unfortunately this actually happens in the real world */
#if 0
	if (node->terminal[saved]) {
		return 0;
# if 0
		errno = EBFONT;
		return -1;
# endif
	}
#endif

	node->terminal[saved] = index + 1;
	return 0;
}

static int
put_map(struct libparsepsf_font *font, size_t index, const uint8_t *seq, size_t seqlen)
{
	uint8_t saved = 0xFF;
	struct libparsepsf_unimap *node = NULL;
	if (put_map_incomplete(font, seq, seqlen, &saved, &node))
		return -1;
	return put_map_finalise(index, saved, node);
}

static int
decode_utf8(const uint8_t *data, size_t size, size_t *np, uint32_t *cpp)
{
	uint8_t head;
	uint32_t cp;

	head = *data;
	*np = 1;
	if (!(head & 0x80)) {
		if (cpp)
			*cpp = (uint32_t)head;
		return 0;
	} else if (!(head & 0x40)) {
		return -1;
	}
	size--;
	cp = (uint32_t)head;
	head <<= 1;

	while (head & 0x80) {
		head <<= 1;
		if ((data[*np] & 0xC0) != 0x80)
			return -1;
		cp <<= 6;
		cp |= (uint32_t)(data[*np] ^ 0x80);
		*np += 1;
	}
	if (*np > 4)
		return -1;

	cp &= (UINT32_C(1) << (*np * 5 + 1)) - 1;
	if ((cp & UINT32_C(0xFFF800)) == UINT32_C(0xD800) ||
	    cp > UINT32_C(0x10FFFF) ||
	    cp < UINT32_C(1) << (*np == 2 ? 7 : *np * 5 - 4))
		return -1;

	if (cpp)
		*cpp = cp;
	return 0;
}

int
libparsepsf_parse_font(const void *data, size_t size, struct libparsepsf_font *fontp, uint32_t *unrecognised_versionp)
{
	union {
		struct psf1_header psf1;
		struct psf2_header psf2;
	} header;
	const uint8_t *udata = data;
	size_t glyphs_offset;
	size_t charsize;
	size_t off;
	size_t i;
	size_t n;
	uint32_t u32;
	uint16_t u16, u16b;
	uint8_t u8, utf8[4], utf8_saved;
	struct libparsepsf_unimap *utf8_node;

	*unrecognised_versionp = 0;
	fontp->glyph_data = NULL;
	fontp->map = NULL;

	if (size < 4)
		goto ebfont;

	if (udata[0] == 0x36) { /* TODO untested */
		if (size < sizeof(header.psf1))
			goto ebfont;
		memcpy(&header.psf1, udata, sizeof(header.psf1));
		if (header.psf1.magic_x36 != 0x36 ||
		    header.psf1.magic_x04 != 0x04)
			goto ebfont;
		fontp->num_glyphs = (header.psf1.mode & PSF1_MODE512) ? 512 : 256;
		fontp->height     = (size_t)header.psf1.height;
		fontp->width      = 8;
		glyphs_offset = sizeof(header.psf1);
		charsize = fontp->height;
		if (glyphs_offset > size ||
		    !fontp->num_glyphs ||
		    charsize > (size - glyphs_offset) / fontp->num_glyphs)
			goto ebfont;
		if (header.psf1.mode & PSF1_MODEHASTAB) {
			off = glyphs_offset + fontp->num_glyphs * charsize;
			for (i = 0; i < fontp->num_glyphs; i++) {
				for (;;) {
					if (off + 2 > size)
						goto ebfont;
					u16 = letoh16(&udata[off]);
					off += 2;
					if (u16 == PSF1_STARTSEQ) {
						break;
					} else if (u16 == PSF1_SEPARATOR) {
						goto next_char_psf1;
					} else if ((u16 & UINT32_C(0xF800)) == UINT32_C(0xD800)) {
						if (off + 2 > size)
							goto ebfont;
						u16b = letoh16(&udata[off]);
						off += 2;
						if (((u16 ^ u16b) & 0xDC00) != 0x0400)
							goto ebfont;
						u32 = desurrogate(u16 < u16b ? u16 : u16b,
						                  u16 < u16b ? u16b : u16);
					} else {
						u32 = (uint32_t)u16;
					}
					n = grapheme_encode_utf8(u32, (char *)utf8, sizeof(utf8));
					if (n > sizeof(utf8))
						abort();
					if (put_map(fontp, i, utf8, n))
						goto fail;
				}
				utf8_saved = 0xFF;
				utf8_node = NULL;
				for (;;) {
					if (off + 2 > size)
						goto ebfont;
					u16 = letoh16(&udata[off]);
					off += 2;
					if (u16 == PSF1_STARTSEQ || u16 == PSF1_SEPARATOR) {
						if (put_map_finalise(i, utf8_saved, utf8_node))
							goto fail;
						if (u16 == PSF1_SEPARATOR)
							goto next_char_psf1;
						utf8_saved = 0xFF;
						utf8_node = NULL;
						continue;
					} else if ((u16 & UINT32_C(0xF800)) == UINT32_C(0xD800)) {
						if (off + 2 > size)
							goto ebfont;
						u16b = letoh16(&udata[off]);
						off += 2;
						if (((u16 ^ u16b) & 0xDC00) != 0x0400)
							goto ebfont;
						u32 = desurrogate(u16 > u16b ? u16 : u16b,
						                  u16 > u16b ? u16b : u16);
					} else {
						u32 = (uint32_t)u16;
					}
					n = grapheme_encode_utf8(u32, (char *)utf8, sizeof(utf8));
					if (n > sizeof(utf8))
						abort();
					if (put_map_incomplete(fontp, utf8, n, &utf8_saved, &utf8_node))
						goto fail;
				}
			next_char_psf1:;
			}
		}

	} else {
		if (size < sizeof(header.psf2))
			goto ebfont;
		memcpy(&header.psf2, udata, sizeof(header.psf2));
		if (header.psf2.magic_x72 != 0x72 ||
		    header.psf2.magic_xb5 != 0xb5 ||
		    header.psf2.magic_x4a != 0x4a ||
		    header.psf2.magic_x86 != 0x86)
			goto ebfont;
		header.psf2.version     = letoh32(header.psf2.version);
		header.psf2.header_size = letoh32(header.psf2.header_size);
		header.psf2.flags       = letoh32(header.psf2.flags);
		header.psf2.num_glyphs  = letoh32(header.psf2.num_glyphs);
		header.psf2.charsize    = letoh32(header.psf2.charsize);
		header.psf2.height      = letoh32(header.psf2.height);
		header.psf2.width       = letoh32(header.psf2.width);
		if (header.psf2.height * ((header.psf2.width + 7) / 8) != header.psf2.charsize)
			goto ebfont;
		if (header.psf2.version > PSF2_MAXVERSION)
			*unrecognised_versionp = header.psf2.version;
		fontp->num_glyphs = (size_t)header.psf2.num_glyphs;
		fontp->height     = (size_t)header.psf2.height;
		fontp->width      = (size_t)header.psf2.width;
		glyphs_offset     = (size_t)header.psf2.header_size;
		charsize = (size_t)header.psf2.charsize;
		if (glyphs_offset > size ||
		    !fontp->num_glyphs ||
		    charsize > (size - glyphs_offset) / fontp->num_glyphs)
			goto ebfont;
		if (header.psf2.flags & PSF2_HAS_UNICODE_TABLE) {
			off = glyphs_offset + fontp->num_glyphs * charsize;
			for (i = 0; i < fontp->num_glyphs; i++) {
				for (;;) {
					if (off == size)
						goto ebfont;
					u8 = udata[off];
					if (u8 == PSF2_STARTSEQ) {
						off += 1;
						break;
					} else if (u8 == PSF2_SEPARATOR) {
						off += 1;
						goto next_char_psf2;
					}
					if (decode_utf8(&udata[off], size - off, &n, NULL))
						goto ebfont;
					if (put_map(fontp, i, &udata[off], n))
						goto fail;
					off += n;
				}
				utf8_saved = 0xFF;
				utf8_node = NULL;
				for (;;) {
					if (off == size)
						goto ebfont;
					u8 = udata[off];
					if (u8 == 0xFE || u8 == 0xFF) {
						if (put_map_finalise(i, utf8_saved, utf8_node))
							goto fail;
						off += 1;
						if (u8 == 0xFF)
							goto next_char_psf2;
						utf8_saved = 0xFF;
						utf8_node = NULL;
					} else {
						if (decode_utf8(&udata[off], size - off, &n, NULL))
							goto ebfont;
						if (put_map_incomplete(fontp, &udata[off], n, &utf8_saved, &utf8_node))
							goto fail;
						off += n;
					}
				}
			next_char_psf2:;
			}
		}
	}

	if (charsize) {
		fontp->glyph_data = malloc(fontp->num_glyphs * charsize);
		if (!fontp->glyph_data)
			goto enomem;
	}
	memcpy(fontp->glyph_data, &udata[glyphs_offset], fontp->num_glyphs * charsize);

	return 0;

enomem:
	errno = ENOMEM;
	goto fail;
ebfont:
	errno = EBFONT;
fail:
	libparsepsf_destroy_font(fontp);
	return -1;
}


size_t
libparsepsf_get_glyph(const struct libparsepsf_font *font, const char *c, size_t *remp, const char **next_cp)
{
	size_t glyph = 0, rem = 0, n;
	uint32_t cp;
	struct libparsepsf_unimap *node = font->map;

	if (!node) {
		if (!remp && !*c)
			return 0;
		if (decode_utf8((const uint8_t *)c, remp ? *remp : SIZE_MAX, &n, &cp)) {
			errno = EILSEQ;
			return 0;
		}
		glyph = (size_t)cp;
		if (glyph >= font->num_glyphs)
			return 0;
		if (next_cp)
			*next_cp = &c[n];
		if (remp)
			*remp = *remp - n;
		return glyph + 1;

	} else if (remp) {
		rem = *remp;
		if (!rem)
			return 0;
		for (; rem > 1; c = &c[1], rem -= 1) {
			if (node->terminal[*(const uint8_t *)c]) {
				glyph = node->terminal[*(const uint8_t *)c];
				if (next_cp)
					*next_cp = &c[1];
				if (remp)
					*remp = rem - 1;
			}
			node = node->nonterminal[*(const uint8_t *)c];
			if (!node)
				return glyph;
		}

	} else {
		if (!c[0])
			return 0;
		for (; c[1]; c = &c[1]) {
			if (node->terminal[*(const uint8_t *)c]) {
				glyph = node->terminal[*(const uint8_t *)c];
				if (next_cp)
					*next_cp = &c[1];
			}
			node = node->nonterminal[*(const uint8_t *)c];
			if (!node)
				return glyph;
		}
	}

	glyph = node->terminal[*(const uint8_t *)c];
	if (glyph) {
		if (next_cp)
			*next_cp = &c[1];
		if (remp)
			*remp = rem - 1;
	}
	return glyph;
}
