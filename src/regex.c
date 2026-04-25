#include "regex.h"

#include "reginold.h"

#include <string.h>

struct Regex {
    const char *source;
    unsigned int options;
    reginold_regex *compiled;
};

static void regex_fill_error(RegexError *err, int code, const char *message, size_t error_offset) {
    if (!err) return;
    err->code = code;
    err->message[0] = '\0';
    err->message_len = 0;
    err->error_offset = error_offset;
    if (!message) return;
    strncpy(err->message, message, sizeof(err->message) - 1);
    err->message[sizeof(err->message) - 1] = '\0';
    err->message_len = strlen(err->message);
}

static unsigned int translate_options(unsigned int options) {
    unsigned int translated = REGINOLD_OPTION_NONE;
    if (options & REGEX_OPTION_IGNORECASE) translated |= REGINOLD_OPTION_IGNORECASE;
    if (options & REGEX_OPTION_MULTILINE) translated |= REGINOLD_OPTION_MULTILINE;
    if (options & REGEX_OPTION_EXTENDED) translated |= REGINOLD_OPTION_EXTENDED;
    return translated;
}

RegexStatus regex_compile(Arena *arena, const char *pattern, unsigned int options, Regex **out, RegexError *err) {
    Regex *regex;
    reginold_error reg_err = {0};
    reginold_status status;

    if (!pattern) {
        regex_fill_error(err, 1, "pattern must not be null", 0);
        return REGEX_ERROR;
    }

    regex = arena_alloc(arena, sizeof(*regex));
    regex->source = pattern;
    regex->options = options;
    regex->compiled = NULL;

    status = reginold_compile(pattern, strlen(pattern), translate_options(options), &regex->compiled, &reg_err);
    if (status != REGINOLD_OK) {
        regex_fill_error(err, reg_err.code, reg_err.message, reg_err.error_offset);
        return status == REGINOLD_MISMATCH ? REGEX_MISMATCH : REGEX_ERROR;
    }

    if (out) *out = regex;
    regex_fill_error(err, 0, NULL, 0);
    return REGEX_OK;
}

const char *regex_source(const Regex *regex) {
    return regex && regex->source ? regex->source : "";
}

RegexStatus regex_search(const Regex *regex, const char *bytes, size_t len, size_t start, RegexMatch *out) {
    reginold_match match = {{0, 0}, 0, NULL};
    reginold_status status;

    if (!regex || !regex->compiled || !bytes || !out || start > len) return REGEX_ERROR;

    status = reginold_search(regex->compiled, bytes, len, start, &match);
    if (status == REGINOLD_OK) {
        out->beg = match.overall.beg;
        out->end = match.overall.end;
        reginold_match_free(&match);
        return REGEX_OK;
    }

    return status == REGINOLD_MISMATCH ? REGEX_MISMATCH : REGEX_ERROR;
}

RegexStatus regex_match_at(const Regex *regex, const char *bytes, size_t len, size_t at, RegexMatch *out) {
    reginold_match match = {{0, 0}, 0, NULL};
    reginold_status status;

    if (!regex || !regex->compiled || !bytes || !out || at > len) return REGEX_ERROR;

    status = reginold_match_at(regex->compiled, bytes, len, at, &match);
    if (status == REGINOLD_OK) {
        out->beg = match.overall.beg;
        out->end = match.overall.end;
        reginold_match_free(&match);
        return REGEX_OK;
    }

    return status == REGINOLD_MISMATCH ? REGEX_MISMATCH : REGEX_ERROR;
}

void regex_match_free(RegexMatch *match) {
    (void)match;
}
