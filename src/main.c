#include "decoder.h"
#include "results.h"

#include <SDL3/SDL_main.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define APP_NAME "Real-Time QR Code Scanner"
#define VIEW_WIDTH 1040
#define VIEW_HEIGHT 640
#define FRAME_TIMEOUT_MS 10000
#define PERMISSION_TIMEOUT_MS 45000

static void usage(FILE *out)
{
    fputs("Usage: qr-scanner [--camera INDEX] [--history FILE | --no-history]\n"
          "       qr-scanner --image FILE.bmp [--history FILE | --no-history]\n"
          "       qr-scanner --list-cameras\n"
          "       qr-scanner --help\n"
          "Default history: scan_history.tsv\n"
          "Window keys: C copies, Space pauses scanning, Esc exits.\n", out);
}

static int show_cameras(const SDL_CameraID *cameras, int count)
{
    if (count == 0) {
        puts("No cameras detected.");
    }
    for (int i = 0; i < count; ++i) {
        const char *name = SDL_GetCameraName(cameras[i]);
        printf("%d: %s\n", i, name ? name : "Unnamed camera");
    }
    return EXIT_SUCCESS;
}

static int scan_frame(Decoder *decoder, Results *results, SDL_Surface *frame)
{
    zbar_image_t *image;
    int count = decoder_scan(decoder, frame, &image);
    if (count < 0) {
        return -1;
    }

    for (const zbar_symbol_t *symbol = zbar_image_first_symbol(image);
         symbol; symbol = zbar_symbol_next(symbol)) {
        if (zbar_symbol_get_count(symbol) == 0 && !results_record(results, symbol)) {
            count = -1;
            break;
        }
    }
    zbar_image_destroy(image);
    return count;
}

static int scan_file(const char *path, Decoder *decoder, Results *results)
{
    SDL_Surface *frame = SDL_LoadBMP(path);
    if (!frame) {
        fprintf(stderr, "Cannot load BMP %s: %s\n", path, SDL_GetError());
        return EXIT_FAILURE;
    }

    int count = scan_frame(decoder, results, frame);
    SDL_DestroySurface(frame);
    if (count < 0) {
        fprintf(stderr, "Scan failed: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }
    if (count == 0) {
        puts("No QR code or barcode found.");
        return 2;
    }
    return EXIT_SUCCESS;
}

static const SDL_FRect video_box = {24, 88, 696, 504};
static const SDL_FRect copy_button = {764, 416, 232, 42};
static const SDL_FRect pause_button = {764, 470, 232, 42};
static const SDL_FRect close_button = {764, 524, 232, 42};

static bool fill(SDL_Renderer *renderer, SDL_FRect rect, Uint8 r, Uint8 g, Uint8 b)
{
    return SDL_SetRenderDrawColor(renderer, r, g, b, 255) &&
           SDL_RenderFillRect(renderer, &rect);
}

static bool label(SDL_Renderer *renderer, float x, float y, const char *text, bool muted)
{
    return (muted ? SDL_SetRenderDrawColor(renderer, 159, 180, 200, 255)
                  : SDL_SetRenderDrawColor(renderer, 238, 247, 255, 255)) &&
           SDL_RenderDebugText(renderer, x, y, text);
}

static bool draw_button(SDL_Renderer *renderer, SDL_FRect rect, const char *text, bool enabled)
{
    return fill(renderer, rect, enabled ? 34 : 31, enabled ? 78 : 43, enabled ? 94 : 57) &&
           label(renderer, rect.x + 14, rect.y + 17, text, !enabled);
}

static bool heading(SDL_Renderer *renderer, float x, float y, const char *text)
{
    if (!SDL_SetRenderScale(renderer, 2.0f, 2.0f)) {
        return false;
    }
    bool drawn = label(renderer, x / 2.0f, y / 2.0f, text, false);
    return SDL_SetRenderScale(renderer, 1.0f, 1.0f) && drawn;
}

static bool draw_preview(SDL_Renderer *renderer, SDL_Texture *texture,
                         int width, int height, const Results *results,
                         const char *status, bool paused)
{
    SDL_FRect panel = {740, 88, 276, 504};
    if (!SDL_SetRenderDrawColor(renderer, 15, 23, 36, 255) || !SDL_RenderClear(renderer) ||
        !fill(renderer, video_box, 8, 13, 21) || !fill(renderer, panel, 24, 37, 54)) {
        return false;
    }

    if (texture) {
        float scale = SDL_min(video_box.w / (float)width, video_box.h / (float)height);
        float video_width = (float)width * scale;
        float video_height = (float)height * scale;
        SDL_FRect destination = {
            video_box.x + (video_box.w - video_width) / 2.0f,
            video_box.y + (video_box.h - video_height) / 2.0f,
            video_width, video_height
        };
        SDL_FRect guide = {
            destination.x + destination.w * 0.12f, destination.y + destination.h * 0.24f,
            destination.w * 0.76f, destination.h * 0.52f
        };
        if (!SDL_RenderTexture(renderer, texture, NULL, &destination) ||
            !SDL_SetRenderDrawColor(renderer, 71, 225, 198, 255) ||
            !SDL_RenderRect(renderer, &guide)) {
            return false;
        }
    }

    char counter[48];
    SDL_snprintf(counter, sizeof counter, "Total scans: %lu", results->count);
    if (!heading(renderer, 24, 22, "QR & BARCODE SCANNER") ||
        !label(renderer, 24, 56, "CAMERA PREVIEW  /  QR, EAN, UPC, CODE 128", true) ||
        !label(renderer, 764, 112, "LATEST RESULT", true) ||
        !label(renderer, 764, 146, results->type ? results->type : "No scans yet", false) ||
        !label(renderer, 764, 308, counter, true)) {
        return false;
    }

    if (results->latest && SDL_strlen(results->latest) <= 14 &&
        strspn(results->latest, "0123456789") == SDL_strlen(results->latest)) {
        if (!heading(renderer, 764, 178, results->latest)) {
            return false;
        }
    } else if (results->latest) {
        const char *cursor = results->latest;
        for (int row = 0; row < 5 && *cursor; ++row) {
            char line[29];
            size_t column = 0;
            while (*cursor && column < sizeof line - 1) {
                Uint32 point = SDL_StepUTF8(&cursor, NULL);
                line[column++] = point < 128 ? (char)point : '?';
            }
            if (row == 4 && *cursor) {
                SDL_memcpy(line + column - 3, "...", 3);
            }
            line[column] = '\0';
            if (!label(renderer, 764, 178.0f + 18.0f * (float)row, line, false)) {
                return false;
            }
        }
        if (!label(renderer, 764, 280, "Full text: title / Copy", true)) {
            return false;
        }
    }

    return label(renderer, 764, 340,
                 results->history_failed ? "History write failed" :
                 results->history ? "History: enabled" : "History: off", true) &&
           draw_button(renderer, copy_button, "Copy result  [C]", results->latest != NULL) &&
           draw_button(renderer, pause_button,
                       paused ? "Resume  [Space]" : "Pause scan  [Space]", true) &&
           draw_button(renderer, close_button, "Close  [Esc]", true) &&
           label(renderer, 24, 611, paused ? "Scanning paused (camera on)." : status, false) &&
           SDL_RenderPresent(renderer);
}

static SDL_Keycode preview_action(SDL_Renderer *renderer, SDL_Event *event)
{
    if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat) {
        return event->key.key;
    }
    if (event->type == SDL_EVENT_MOUSE_BUTTON_UP && event->button.button == SDL_BUTTON_LEFT &&
        SDL_ConvertEventToRenderCoordinates(renderer, event)) {
        SDL_FPoint point = {event->button.x, event->button.y};
        if (SDL_PointInRectFloat(&point, &copy_button)) {
            return SDLK_C;
        }
        if (SDL_PointInRectFloat(&point, &pause_button)) {
            return SDLK_SPACE;
        }
        if (SDL_PointInRectFloat(&point, &close_button)) {
            return SDLK_ESCAPE;
        }
    }
    return 0;
}

static bool run_preview(SDL_Camera *camera, SDL_CameraID id,
                        SDL_Window *window, SDL_Renderer *renderer,
                        Decoder *decoder, Results *results)
{
    SDL_Texture *texture = NULL;
    int width = 0;
    int height = 0;
    Uint64 last_frame = SDL_GetTicks();
    bool permission_granted = false;
    bool paused = false;
    const char *status = "Waiting for camera permission...";
    bool ok = draw_preview(renderer, NULL, 0, 0, results, status, paused);

    while (ok) {
        SDL_Event event;
        bool redraw = false;
        if (SDL_WaitEventTimeout(&event, 10)) {
            do {
                SDL_Keycode action = preview_action(renderer, &event);
                if (event.type == SDL_EVENT_QUIT ||
                    event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED ||
                    action == SDLK_ESCAPE) {
                    goto done;
                }
                if (event.type == SDL_EVENT_CAMERA_DEVICE_REMOVED && event.cdevice.which == id) {
                    ok = SDL_SetError("Camera disconnected");
                    goto done;
                }
                if (action == SDLK_SPACE) {
                    paused = !paused;
                    redraw = true;
                }
                if (action == SDLK_C && results->latest) {
                    status = SDL_SetClipboardText(results->clipboard)
                             ? "Latest result copied" : "Could not copy result";
                    redraw = true;
                }
                if (event.type == SDL_EVENT_WINDOW_EXPOSED ||
                    event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                    redraw = true;
                }
            } while (SDL_PollEvent(&event));
        }

        int permission = SDL_GetCameraPermissionState(camera);
        if (permission < 0) {
            ok = SDL_SetError("Camera access denied. Check your system camera permissions");
            break;
        }
        if (permission == 0 && SDL_GetTicks() - last_frame >= PERMISSION_TIMEOUT_MS) {
            ok = SDL_SetError("Camera permission did not resolve. Allow camera access and retry");
            break;
        }
        if (permission > 0 && !permission_granted) {
            permission_granted = true;
            last_frame = SDL_GetTicks();
            status = "Waiting for the first frame...";
            redraw = true;
        }

        SDL_Surface *frame = permission_granted ? SDL_AcquireCameraFrame(camera, NULL) : NULL;
        if (frame) {
            bool first_frame = texture == NULL;
            if (!texture || width != frame->w || height != frame->h) {
                SDL_DestroyTexture(texture);
                width = frame->w;
                height = frame->h;
                texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24,
                                            SDL_TEXTUREACCESS_STREAMING, width, height);
            }
            unsigned long previous_count = results->count;
            ok = texture && SDL_UpdateTexture(texture, NULL, frame->pixels, frame->pitch) &&
                 (paused || scan_frame(decoder, results, frame) >= 0);
            SDL_ReleaseCameraFrame(camera, frame);
            if (!ok) {
                break;
            }
            if (first_frame) {
                fprintf(stderr, "Camera streaming (%d x %d). Press Esc to exit.\n", width, height);
            }

            last_frame = SDL_GetTicks();
            if (results->count != previous_count) {
                char *title = NULL;
                if (SDL_asprintf(&title, "%s | %s", results->type, results->latest) >= 0) {
                    SDL_SetWindowTitle(window, title);
                    SDL_free(title);
                }
                status = "Code detected. Hold the next label in view.";
            } else if (first_frame) {
                status = "Center the entire QR code or barcode. Tilt the label to avoid glare.";
            }
            redraw = true;
        } else if (permission_granted && SDL_GetTicks() - last_frame >= FRAME_TIMEOUT_MS) {
            ok = SDL_SetError("Camera sent no frames for 10 seconds. Check the device and retry");
            break;
        }

        if (redraw) {
            ok = draw_preview(renderer, texture, width, height, results, status, paused);
        }
    }

done:
    if (!ok) {
        fprintf(stderr, "Scanner stopped: %s\n", SDL_GetError());
    }
    SDL_DestroyTexture(texture);
    return ok;
}

static int scan_camera(SDL_CameraID id, Decoder *decoder, Results *results)
{
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    if (!SDL_CreateWindowAndRenderer(APP_NAME, VIEW_WIDTH, VIEW_HEIGHT,
                                     SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY,
                                     &window, &renderer)) {
        fprintf(stderr, "Cannot create preview: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    int status = EXIT_FAILURE;
    if (!SDL_SetRenderLogicalPresentation(renderer, VIEW_WIDTH, VIEW_HEIGHT,
                                          SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
        fprintf(stderr, "Cannot configure preview: %s\n", SDL_GetError());
        goto done;
    }

    SDL_CameraSpec spec = {
        .format = SDL_PIXELFORMAT_RGB24,
        .colorspace = SDL_COLORSPACE_SRGB,
        .width = 640,
        .height = 480,
        .framerate_numerator = 30,
        .framerate_denominator = 1
    };
    SDL_Camera *camera = SDL_OpenCamera(id, &spec);
    if (!camera) {
        fprintf(stderr, "Cannot open camera: %s\n", SDL_GetError());
        goto done;
    }

    fprintf(stderr, "Opening camera. Allow access if your system asks.\n");
    if (run_preview(camera, id, window, renderer, decoder, results)) {
        status = EXIT_SUCCESS;
    }
    SDL_CloseCamera(camera);

done:
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    return status;
}

int main(int argc, char **argv)
{
    const char *image = NULL;
    const char *history_path = "scan_history.tsv";
    bool history_option = false;
    bool camera_option = false;
    bool list = false;
    long camera_index = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 && argc == 2) {
            usage(stdout);
            return EXIT_SUCCESS;
        }
        if (strcmp(argv[i], "--list-cameras") == 0 && argc == 2) {
            list = true;
        } else if (strcmp(argv[i], "--image") == 0 && !image && i + 1 < argc) {
            image = argv[++i];
        } else if (strcmp(argv[i], "--camera") == 0 && !camera_option && i + 1 < argc) {
            const char *value = argv[++i];
            char *end;
            errno = 0;
            camera_index = strtol(value, &end, 10);
            if (value[0] < '0' || value[0] > '9' || *end != '\0' || errno == ERANGE) {
                usage(stderr);
                return EXIT_FAILURE;
            }
            camera_option = true;
        } else if (strcmp(argv[i], "--history") == 0 && !history_option && i + 1 < argc) {
            history_path = argv[++i];
            history_option = true;
        } else if (strcmp(argv[i], "--no-history") == 0 && !history_option) {
            history_path = NULL;
            history_option = true;
        } else {
            usage(stderr);
            return EXIT_FAILURE;
        }
    }

    if (image && camera_option) {
        usage(stderr);
        return EXIT_FAILURE;
    }

    SDL_SetAppMetadata(APP_NAME, "1.0.0", "edu.qrscanner");
    SDL_InitFlags flags = image ? 0 : SDL_INIT_CAMERA;
    if (!image && !list) {
        flags |= SDL_INIT_VIDEO;
    }
    if (!SDL_Init(flags)) {
        fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    int status = EXIT_FAILURE;
    SDL_CameraID *cameras = NULL;
    Decoder decoder = {0};
    Results results = {0};
    if (!image) {
        int count;
        cameras = SDL_GetCameras(&count);
        if (!cameras) {
            fprintf(stderr, "Cannot list cameras: %s\n", SDL_GetError());
            goto done;
        }
        if (list) {
            status = show_cameras(cameras, count);
            goto done;
        }
        if (count == 0) {
            fprintf(stderr, "No camera detected. Connect a camera and try again.\n");
            goto done;
        }
        if (camera_index >= count) {
            fprintf(stderr, "Camera %ld does not exist. Run --list-cameras.\n", camera_index);
            goto done;
        }
    }

    if (history_path) {
        results.history = fopen(history_path, "a");
        if (!results.history) {
            fprintf(stderr, "Cannot open history %s: %s\n", history_path, strerror(errno));
            goto done;
        }
    }
    if (!decoder_init(&decoder, !image)) {
        fprintf(stderr, "%s\n", SDL_GetError());
        goto done;
    }

    if (image) {
        status = scan_file(image, &decoder, &results);
    } else {
        status = scan_camera(cameras[camera_index], &decoder, &results);
    }

done:
    if (results.history && fclose(results.history) == EOF && !results.history_failed) {
        perror("Cannot close scan history");
        results.history_failed = true;
    }
    if (results.history_failed) {
        status = EXIT_FAILURE;
    }
    SDL_free(results.latest);
    SDL_free(results.clipboard);
    decoder_destroy(&decoder);
    SDL_free(cameras);
    SDL_Quit();
    return status;
}
