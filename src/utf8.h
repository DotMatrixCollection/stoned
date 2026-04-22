#ifndef STONED_UTF8_H
#define STONED_UTF8_H

#include <stddef.h>
#include <stdint.h>

int utf8_validate(const char *s, size_t len, size_t *error_offset);
int utf8_decode_one(const char *s, size_t len, uint32_t *codepoint, size_t *width);
size_t utf8_char_count(const char *s);
int utf8_char_at(const char *s, size_t index, const char **ptr, size_t *width, uint32_t *codepoint);
size_t utf8_prev_char_start(const char *s, size_t end);
size_t utf8_byte_offset_for_char(const char *s, size_t char_index);
size_t utf8_char_index_for_byte(const char *s, size_t byte_offset);
int utf8_ascii_only(const char *s);

#endif
