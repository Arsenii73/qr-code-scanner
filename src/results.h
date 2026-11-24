#ifndef QR_RESULTS_H
#define QR_RESULTS_H

#include <SDL3/SDL.h>
#include <stdio.h>
#include <zbar.h>

typedef struct {
    FILE *history;
    char *latest;
    char *clipboard;
    const char *type;
    unsigned long count;
    bool history_failed;
} Results;

bool results_record(Results *results, const zbar_symbol_t *symbol);

#endif
