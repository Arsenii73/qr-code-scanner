#include "decoder.h"

#include <limits.h>
#include <stdint.h>

bool decoder_init(Decoder *decoder, bool live)
{
    decoder->scanner = zbar_image_scanner_create();
    if (!decoder->scanner ||
        zbar_image_scanner_set_config(decoder->scanner, ZBAR_NONE, ZBAR_CFG_ENABLE, 0)) {
        return SDL_SetError("Cannot initialize ZBar decoder");
    }

    const zbar_symbol_type_t formats[] = {
        ZBAR_QRCODE, ZBAR_EAN13, ZBAR_EAN8, ZBAR_UPCA, ZBAR_UPCE, ZBAR_CODE128
    };
    for (size_t i = 0; i < sizeof formats / sizeof formats[0]; ++i) {
        if (zbar_image_scanner_set_config(decoder->scanner, formats[i], ZBAR_CFG_ENABLE, 1)) {
            return SDL_SetError("Cannot enable ZBar format %s", zbar_get_symbol_name(formats[i]));
        }
    }

    zbar_image_scanner_enable_cache(decoder->scanner, live);
    return true;
}

/* Recover dark modules where glare or soft focus reduces local contrast. */
static void sharpen_and_threshold(const unsigned char *gray, unsigned char *out,
                                  int width, int height, size_t size)
{
    size_t histogram[256] = {0};
    for (size_t i = 0; i < size; ++i) {
        ++histogram[gray[i]];
    }

    size_t tail = size / 100;
    int low = 0;
    int high = 255;
    for (size_t n = histogram[low]; n <= tail && low < 255;) {
        n += histogram[++low];
    }
    for (size_t n = histogram[high]; n <= tail && high > low;) {
        n += histogram[--high];
    }
    int threshold = (low + high) / 2;
    size_t stride = (size_t)width;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t i = (size_t)y * stride + (size_t)x;
            int value = gray[i];
            if (x > 0 && x < width - 1 && y > 0 && y < height - 1) {
                int neighbors = gray[i - 1] + gray[i + 1] +
                                gray[i - stride] + gray[i + stride];
                value = 3 * value - neighbors / 2;
            }
            out[i] = value > threshold ? 255 : 0;
        }
    }
}

int decoder_scan(Decoder *decoder, SDL_Surface *frame, zbar_image_t **result)
{
    *result = NULL;
    if (frame->w <= 0 || frame->h <= 0 ||
        (size_t)frame->w > SIZE_MAX / (size_t)frame->h ||
        (size_t)frame->w * (size_t)frame->h > ULONG_MAX ||
        (size_t)frame->w * (size_t)frame->h > SIZE_MAX / 2) {
        SDL_SetError("Invalid image dimensions");
        return -1;
    }

    size_t size = (size_t)frame->w * (size_t)frame->h;
    if (size > decoder->capacity) {
        unsigned char *gray = SDL_realloc(decoder->gray, size * 2);
        if (!gray) {
            SDL_SetError("Cannot allocate grayscale image");
            return -1;
        }
        decoder->gray = gray;
        decoder->capacity = size;
    }

    SDL_Surface *rgb = frame;
    if (frame->format != SDL_PIXELFORMAT_RGB24) {
        rgb = SDL_ConvertSurface(frame, SDL_PIXELFORMAT_RGB24);
        if (!rgb) {
            return -1;
        }
    }

    if (!SDL_LockSurface(rgb)) {
        if (rgb != frame) {
            SDL_DestroySurface(rgb);
        }
        return -1;
    }

    /* SDL rows can include padding. ZBar needs tightly packed luminance. */
    for (int y = 0; y < rgb->h; ++y) {
        const unsigned char *pixel = (const unsigned char *)rgb->pixels +
                                     (size_t)y * (size_t)rgb->pitch;
        unsigned char *gray = decoder->gray + (size_t)y * (size_t)rgb->w;
        for (int x = 0; x < rgb->w; ++x, pixel += 3) {
            gray[x] = (unsigned char)((77u * pixel[0] + 150u * pixel[1] +
                                      29u * pixel[2] + 128u) >> 8);
        }
    }
    SDL_UnlockSurface(rgb);
    if (rgb != frame) {
        SDL_DestroySurface(rgb);
    }

    zbar_image_t *image = zbar_image_create();
    if (!image) {
        SDL_SetError("Cannot allocate ZBar image");
        return -1;
    }
    zbar_image_set_format(image, zbar_fourcc('Y', '8', '0', '0'));
    zbar_image_set_size(image, (unsigned)frame->w, (unsigned)frame->h);
    zbar_image_set_data(image, decoder->gray, (unsigned long)size, NULL);

    int count = zbar_scan_image(decoder->scanner, image);
    const zbar_symbol_set_t *symbols = zbar_image_get_symbols(image);
    if (count == 0 && (!symbols || !zbar_symbol_set_first_unfiltered(symbols))) {
        unsigned char *enhanced = decoder->gray + size;
        sharpen_and_threshold(decoder->gray, enhanced, frame->w, frame->h, size);
        zbar_image_set_data(image, enhanced, (unsigned long)size, NULL);
        count = zbar_scan_image(decoder->scanner, image);
    }
    if (count < 0) {
        zbar_image_destroy(image);
        SDL_SetError("ZBar could not scan the image");
        return -1;
    }
    *result = image;
    return count;
}

void decoder_destroy(Decoder *decoder)
{
    if (decoder->scanner) {
        zbar_image_scanner_destroy(decoder->scanner);
    }
    SDL_free(decoder->gray);
}
