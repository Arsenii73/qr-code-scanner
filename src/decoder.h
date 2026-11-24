#ifndef QR_DECODER_H
#define QR_DECODER_H

#include <SDL3/SDL.h>
#include <zbar.h>

typedef struct {
    zbar_image_scanner_t *scanner;
    unsigned char *gray;
    size_t capacity;
} Decoder;

bool decoder_init(Decoder *decoder, bool live);
int decoder_scan(Decoder *decoder, SDL_Surface *frame, zbar_image_t **result);
void decoder_destroy(Decoder *decoder);

#endif
