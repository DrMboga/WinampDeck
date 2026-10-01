// The one translation unit that compiles stb_image's implementation. Covers
// are always JPEG, so every other format is left out.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#if defined(__ARM_NEON)
#define STBI_NEON  // Off by default on ARM; the Pi 3's Cortex-A53 has it.
#endif
#include <stb_image.h>
