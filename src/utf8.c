#include "utf8.h"

int utf8_decode_one(const char *s, size_t len, uint32_t *codepoint, size_t *width) {
    unsigned char c0;
    uint32_t cp;
    size_t need;

    if (!s || len == 0) return 0;
    c0 = (unsigned char)s[0];
    if (c0 == 0) return 0;

    if (c0 < 0x80) {
        cp = c0;
        need = 1;
    } else if (c0 >= 0xC2 && c0 <= 0xDF) {
        cp = (uint32_t)(c0 & 0x1F);
        need = 2;
    } else if (c0 >= 0xE0 && c0 <= 0xEF) {
        cp = (uint32_t)(c0 & 0x0F);
        need = 3;
    } else if (c0 >= 0xF0 && c0 <= 0xF4) {
        cp = (uint32_t)(c0 & 0x07);
        need = 4;
    } else {
        return 0;
    }

    if (need > len) return 0;
    for (size_t i = 1; i < need; i++) {
        unsigned char cx = (unsigned char)s[i];
        if ((cx & 0xC0) != 0x80) return 0;
        cp = (cp << 6) | (uint32_t)(cx & 0x3F);
    }

    if ((need == 2 && cp < 0x80) ||
        (need == 3 && cp < 0x800) ||
        (need == 4 && cp < 0x10000)) return 0;
    if (cp > 0x10FFFF) return 0;
    if (cp >= 0xD800 && cp <= 0xDFFF) return 0;
    if ((need == 3 && c0 == 0xE0 && (unsigned char)s[1] < 0xA0) ||
        (need == 3 && c0 == 0xED && (unsigned char)s[1] >= 0xA0) ||
        (need == 4 && c0 == 0xF0 && (unsigned char)s[1] < 0x90) ||
        (need == 4 && c0 == 0xF4 && (unsigned char)s[1] >= 0x90)) return 0;

    if (codepoint) *codepoint = cp;
    if (width) *width = need;
    return 1;
}

size_t utf8_encode_one(uint32_t codepoint, char out[4]) {
    if (codepoint <= 0x7F) {
        out[0] = (char)codepoint;
        return 1;
    }
    if (codepoint <= 0x7FF) {
        out[0] = (char)(0xC0 | (codepoint >> 6));
        out[1] = (char)(0x80 | (codepoint & 0x3F));
        return 2;
    }
    if (codepoint <= 0xFFFF) {
        out[0] = (char)(0xE0 | (codepoint >> 12));
        out[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        out[2] = (char)(0x80 | (codepoint & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (codepoint >> 18));
    out[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
    out[3] = (char)(0x80 | (codepoint & 0x3F));
    return 4;
}

int utf8_validate(const char *s, size_t len, size_t *error_offset) {
    size_t i = 0;
    while (i < len) {
        size_t width = 0;
        if (!utf8_decode_one(s + i, len - i, NULL, &width)) {
            if (error_offset) *error_offset = i;
            return 0;
        }
        i += width;
    }
    return 1;
}

size_t utf8_char_count(const char *s) {
    size_t count = 0;
    size_t i = 0;
    size_t len = 0;
    while (s[len]) len++;
    while (i < len) {
        size_t width = 0;
        if (!utf8_decode_one(s + i, len - i, NULL, &width)) return count;
        i += width;
        count++;
    }
    return count;
}

int utf8_char_at(const char *s, size_t index, const char **ptr, size_t *width, uint32_t *codepoint) {
    size_t cur = 0;
    size_t i = 0;
    size_t len = 0;
    while (s[len]) len++;
    while (i < len) {
        size_t w = 0;
        uint32_t cp = 0;
        if (!utf8_decode_one(s + i, len - i, &cp, &w)) return 0;
        if (cur == index) {
            if (ptr) *ptr = s + i;
            if (width) *width = w;
            if (codepoint) *codepoint = cp;
            return 1;
        }
        i += w;
        cur++;
    }
    return 0;
}

size_t utf8_prev_char_start(const char *s, size_t end) {
    if (end == 0) return 0;
    size_t i = end - 1;
    while (i > 0 && (((unsigned char)s[i]) & 0xC0) == 0x80) i--;
    return i;
}

size_t utf8_byte_offset_for_char(const char *s, size_t char_index) {
    size_t cur = 0;
    size_t i = 0;
    size_t len = 0;
    while (s[len]) len++;
    while (i < len && cur < char_index) {
        size_t w = 0;
        if (!utf8_decode_one(s + i, len - i, NULL, &w)) return i;
        i += w;
        cur++;
    }
    return i;
}

size_t utf8_char_index_for_byte(const char *s, size_t byte_offset) {
    size_t cur = 0;
    size_t i = 0;
    size_t len = 0;
    while (s[len]) len++;
    if (byte_offset > len) byte_offset = len;
    while (i < byte_offset) {
        size_t w = 0;
        if (!utf8_decode_one(s + i, len - i, NULL, &w)) return cur;
        i += w;
        cur++;
    }
    return cur;
}

int utf8_ascii_only(const char *s) {
    for (size_t i = 0; s[i]; i++) {
        if ((unsigned char)s[i] >= 0x80) return 0;
    }
    return 1;
}
