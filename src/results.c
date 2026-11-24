#include "results.h"

#include <string.h>
#include <time.h>

static const char *classify(const char *data, size_t size)
{
    if ((size > 8 && SDL_strncasecmp(data, "https://", 8) == 0) ||
        (size > 7 && SDL_strncasecmp(data, "http://", 7) == 0)) {
        return "URL";
    }
    if (size >= 5 && SDL_strncasecmp(data, "WIFI:", 5) == 0) {
        return "Wi-Fi";
    }
    if ((size >= 11 && SDL_strncasecmp(data, "BEGIN:VCARD", 11) == 0) ||
        (size >= 7 && SDL_strncasecmp(data, "MECARD:", 7) == 0)) {
        return "Contact";
    }
    if (size >= 7 && SDL_strncasecmp(data, "mailto:", 7) == 0) {
        return "Email";
    }
    if (size >= 4 && SDL_strncasecmp(data, "tel:", 4) == 0) {
        return "Phone";
    }
    return "Text";
}

/* Keep each result on one line. Preserve valid UTF-8 and escape binary bytes. */
static char *escape_payload(const char *data, size_t size, bool *is_text)
{
    *is_text = true;
    if (size > (SDL_SIZE_MAX - 1) / 4) {
        return NULL;
    }
    char *escaped = SDL_malloc(size * 4 + 1);
    if (!escaped) {
        return NULL;
    }

    static const char hex[] = "0123456789ABCDEF";
    char *out = escaped;
    const char *cursor = data;
    size_t remaining = size;
    while (remaining > 0) {
        const char *start = cursor;
        Uint32 codepoint = SDL_StepUTF8(&cursor, &remaining);
        if (codepoint == 0) {
            *is_text = false;
            ++cursor;
            --remaining;
        }
        size_t length = (size_t)(cursor - start);
        bool invalid = codepoint == SDL_INVALID_UNICODE_CODEPOINT && length == 1;
        if (invalid) {
            *is_text = false;
        }
        if (codepoint == '\\') {
            *out++ = '\\';
            *out++ = '\\';
        } else if (codepoint < 32 || (codepoint >= 127 && codepoint <= 159) || invalid) {
            for (size_t i = 0; i < length; ++i) {
                unsigned char byte = (unsigned char)start[i];
                *out++ = '\\';
                *out++ = 'x';
                *out++ = hex[byte >> 4];
                *out++ = hex[byte & 15];
            }
        } else {
            memcpy(out, start, length);
            out += length;
        }
    }
    *out = '\0';
    return escaped;
}

bool results_record(Results *results, const zbar_symbol_t *symbol)
{
    const char *data = zbar_symbol_get_data(symbol);
    size_t size = zbar_symbol_get_data_length(symbol);
    bool is_text;
    char *escaped = escape_payload(data, size, &is_text);
    if (!escaped) {
        return SDL_SetError("Cannot store decoded result");
    }

    char *clipboard = is_text ? SDL_strndup(data, size) : SDL_strdup(escaped);
    if (!clipboard) {
        SDL_free(escaped);
        return SDL_SetError("Cannot store clipboard text");
    }
    SDL_free(results->clipboard);
    results->clipboard = clipboard;
    SDL_free(results->latest);
    results->latest = escaped;
    zbar_symbol_type_t format = zbar_symbol_get_type(symbol);
    results->type = format == ZBAR_QRCODE ? classify(data, size) : zbar_get_symbol_name(format);
    ++results->count;
    printf("[%s] %s\n", results->type, results->latest);
    fflush(stdout);

    if (results->history && !results->history_failed) {
        char timestamp[24] = "unknown";
        time_t now = time(NULL);
        struct tm *utc = gmtime(&now);
        if (utc) {
            strftime(timestamp, sizeof timestamp, "%Y-%m-%dT%H:%M:%SZ", utc);
        }

        if (fprintf(results->history, "%s\t%s\t%s\n",
                    timestamp, results->type, results->latest) < 0 ||
            fflush(results->history) == EOF) {
            perror("Cannot write scan history");
            results->history_failed = true;
        }
    }
    return true;
}
