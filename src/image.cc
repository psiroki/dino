#include "image.hh"

#include <stdio.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

extern Platform platform;

static SDL_Surface* finishLoad(const char *fn, unsigned char *data, int width, int height, int channels);

SDL_Surface* loadImage(const char* filename) {
    int width, height, channels;
    unsigned char *data = stbi_load(filename, &width, &height, &channels, STBI_rgb_alpha);

    return finishLoad(filename, data, width, height, channels);
}

SDL_Surface* loadImageFromMemory(const void* contents, int size) {
    int width, height, channels;
    unsigned char *data = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(contents), size, &width, &height, &channels, STBI_rgb_alpha);

    return finishLoad("<memory>", data, width, height, channels);
}

static SDL_Surface* finishLoad(const char *fn, unsigned char *data, int width, int height, int channels) {
    if (data == NULL) {
        fprintf(stderr, "Failed to load image: %s (%s)\n", stbi_failure_reason(), fn);
        return NULL;
    }

    // SDL interprets each pixel as a 32-bit number, so we need an SDL_Surface to match.
    SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(
        data,  // The image pixel data
        width, // Width of the image
        height, // Height of the image
        32, // Bits per pixel (8 bits per channel * 4 channels = 32 bits)
        width * 4, // The pitch (number of bytes in a row of the image)
        0x000000ff, // Red mask
        0x0000ff00, // Green mask
        0x00ff0000, // Blue mask
        0xff000000  // Alpha mask
    );

    if (surface == NULL) {
        fprintf(stderr, "Failed to create SDL surface: %s\n", SDL_GetError());
        stbi_image_free(data);
        return NULL;
    }

    // Free the original data when the SDL_Surface is freed
    SDL_Surface* optimizedSurface = platform.displayFormat(surface);
    SDL_FreeSurface(surface);
    stbi_image_free(data);

    return optimizedSurface;
}
