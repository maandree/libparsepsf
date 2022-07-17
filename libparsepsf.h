/* See LICENSE file for copyright and license details. */
#ifndef LIBPARSEPSF_H
#define LIBPARSEPSF_H

#include <stdint.h>
#include <stddef.h>


/**
 * Glyph trie
 */
struct libparsepsf_unimap {

	/**
	 * Mapping for next byte in a longer sequences
	 */
	struct libparsepsf_unimap *nonterminal[256];

	/**
	 * Unless the byte index maps to 0, mapped to value
	 * less 1 is the glyph index to use for the sequence
	 * if there is no longer sequence
	 */
	size_t terminal[256];
};

/**
 * Parsed PSF font structure
 */
struct libparsepsf_font {

	/**
	 * The number of glyphs in the font
	 */
	size_t num_glyphs;

	/**
	 * The bit-height of each glyph
	 */
	size_t height;

	/**
	 * The bit-width of each glyph
	 */
	size_t width;

	/**
	 * The glyph data
	 * 
	 * Each glyph is `.height * (.width / 8 + (.width % 8 ? 1 : 0))`
	 * bytes large. Each glyph is right-padded to a multiple of 8 bits,
	 * and are oriented in row-major and most-signficant-bit-first order,
	 * meaning that the bit 0x80 in the first byte for a glyph represents
	 * the left-most, top-most bit in the, and 0x40 in the same byte
	 * represents the bit directly right to it. Bit on means ink on,
	 * bit off means ink off.
	 */
	uint8_t *glyph_data;

	/**
	 * Glyph trie, maps from byte sequences to glyph indices;
	 * you can use `libparsepsf_get_glyph` to search it
	 * (enumeration has to be done manually); not that an
	 * entry can be multiple characters wide, for example
	 * to deal with context dependent glyphs and grapheme
	 * clusters
	 * 
	 * `NULL` if unicode character points map directly
	 * (identity mapping) to glyph indices; and UTF-8 the
	 * assumed encoding
	 */
	struct libparsepsf_unimap *map;
};


/**
 * Deallocate font
 * 
 * @param  font  Pointer to the data to deallocate
 */
void libparsepsf_destroy_font(struct libparsepsf_font *font);

/**
 * Parse a PSF font file
 * 
 * @param   data                   The font file content
 * @param   size                   The number of bytes in `data`
 * @param   fontp                  Output parameter for the font, should be deallocated
 *                                 using `libparsepsf_destroy_font` when no longer
 *                                 needed (only allocated on success completion)
 * @param   unrecognised_versionp  Normally set to 0; set to 1 if the minor version in
 *                                 the font file is unrecognised (backwards-compatibility
 *                                 is assumed, so it will still be parsed as a supported
 *                                 font file)
 * @return                         0 on successful completion, -1 on failure
 * @throws  ENOMEM                 Failed to allocate enough memory
 * @throws  EBFONT                 Corrupt or unsupported font file
 */
int libparsepsf_parse_font(const void *data, size_t size, struct libparsepsf_font *fontp, uint32_t *unrecognised_versionp);

/**
 * Get the next glyph to print for a text
 *
 * @param   font     The parsed font, created with `libparsepsf_parse_font`
 * @param   c        The current position in the text (the text shall be
 *                   completely loaded to guarantee multi-characters glyphs
 *                   are properly printed)
 * @param   remp     The number of bytes in `c`, will be updated to reflect
 *                   the value it shall have when the function is called
 *                   again, with `*next_cp` as `c` (that is, the number of
 *                   bytes in the found byte sequence is subtracted);
 *                   if `NULL`, the text ends when a NUL byte is found
 * @param   next_cp  Output parameter for the next position in the text;
 *                   may be `NULL`
 * @return           The index of the glyph, plus 1; 0 if the glyph if the
 *                   end of the text is reached or if no glyph is found,
 *                   or (only if `font->map` is `NULL`) if an illegal byte
 *                   sequence was found. `*remp` and `*next_cp` are not
 *                   updated if this function returns 0.
 * @throws  EILSEQ   If an illegal byte sequence was found (only if
 *                   `font->map` is `NULL`)
 */
size_t libparsepsf_get_glyph(const struct libparsepsf_font *font, const char *c, size_t *remp, const char **next_cp);

#endif
