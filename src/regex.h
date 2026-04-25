#ifndef STONED_REGEX_H
#define STONED_REGEX_H

#include "arena.h"

#include <stddef.h>

typedef struct Regex Regex;

typedef enum {
    REGEX_OK = 0,
    REGEX_MISMATCH = -1,
    REGEX_ERROR = -2
} RegexStatus;

#define REGEX_OPTION_NONE       0u
#define REGEX_OPTION_IGNORECASE (1u << 0)
#define REGEX_OPTION_MULTILINE  (1u << 1)
#define REGEX_OPTION_EXTENDED   (1u << 2)
#define REGEX_ERROR_MSG_MAX     90

typedef struct {
    int code;
    char message[REGEX_ERROR_MSG_MAX];
    size_t message_len;
    size_t error_offset;
} RegexError;

typedef struct {
    long    beg;
    long    end;
    size_t  capture_count;
    long   *cap_beg;  /* malloc'd; [i] = start of group i+1; -1 if unmatched */
    long   *cap_end;  /* malloc'd; [i] = end   of group i+1; -1 if unmatched */
} RegexMatch;

RegexStatus regex_compile(Arena *arena, const char *pattern, unsigned int options, Regex **out, RegexError *err);
const char *regex_source(const Regex *regex);
RegexStatus regex_search(const Regex *regex, const char *bytes, size_t len, size_t start, RegexMatch *out);
RegexStatus regex_match_at(const Regex *regex, const char *bytes, size_t len, size_t at, RegexMatch *out);
void regex_match_free(RegexMatch *match);

#endif
