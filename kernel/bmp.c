#include <kernel/multiboot.h>
#include <kernel/time.h>

// Symbols generated automatically by objcopy
extern char _binary_kernel_video_logo_bmp_start[];

void draw_bmp(struct multiboot_info *mbi) {
    if (!(mbi->flags & (1 << 12)) || mbi->framebuffer_type != 1) {
        return;
    }

    volatile uint8_t *fb = (volatile uint8_t *)(uintptr_t)mbi->framebuffer_addr;
    uint32_t pitch = mbi->framebuffer_pitch;
    uint32_t screen_width = mbi->framebuffer_width;
    uint32_t screen_height = mbi->framebuffer_height;
    uint32_t bytes_per_pixel = mbi->framebuffer_bpp / 8;

    // Point directly to the raw binary data loaded in memory
    unsigned char *logo_bmp = (unsigned char *)_binary_kernel_video_logo_bmp_start;

    uint32_t data_offset = *(uint32_t *)&logo_bmp[10];
    int32_t img_width    = *(int32_t *)&logo_bmp[18];
    int32_t img_height   = *(int32_t *)&logo_bmp[22];
    uint16_t bpp         = *(uint16_t *)&logo_bmp[28];

    if (logo_bmp[0] != 'B' || logo_bmp[1] != 'M' || bpp != 8 ||
        bytes_per_pixel == 0 || bytes_per_pixel > 4) {
        return;
    }

    unsigned char *pixel_data = logo_bmp + data_offset;
    unsigned char *palette = logo_bmp + 54;

    int start_x = (screen_width - img_width) / 2;
    int start_y = (screen_height - (img_height < 0 ? -img_height : img_height)) / 2;

    int absolute_height = (img_height < 0) ? -img_height : img_height;
    int is_bottom_up = (img_height > 0);
    int row_stride = ((img_width * (bpp / 8) + 3) & ~3);

    for (int y = 0; y < absolute_height; y++) {
        int src_y = is_bottom_up ? (absolute_height - 1 - y) : y;
        int screen_y = start_y + y;

        if (screen_y < 0 || screen_y >= (int)screen_height) continue;

        unsigned char *row_ptr = pixel_data + (src_y * row_stride);

        for (int x = 0; x < img_width; x++) {
            int screen_x = start_x + x;
            if (screen_x < 0 || screen_x >= (int)screen_width) continue;

            unsigned char *entry = palette + row_ptr[x] * 4;
            uint32_t color = (entry[2] << 16) | (entry[1] << 8) | entry[0];
            volatile uint8_t *screen_pixel = fb + screen_y * pitch + screen_x * bytes_per_pixel;

            for (uint32_t byte = 0; byte < bytes_per_pixel; byte++) {
                screen_pixel[byte] = (uint8_t)(color >> (byte * 8));
            }
        }
    }
}

void fade_bmp_to_dust(struct multiboot_info *mbi) {
    volatile uint8_t *fb;
    unsigned char *logo_bmp;
    unsigned char *pixel_data;
    unsigned char *palette;
    uint32_t pitch;
    uint32_t screen_width;
    uint32_t screen_height;
    uint32_t bytes_per_pixel;
    uint32_t data_offset;
    int32_t img_width;
    int32_t img_height;
    uint16_t bpp;
    int absolute_height;
    int row_stride;
    int start_x;
    int start_y;

    if (!(mbi->flags & (1 << 12)) || mbi->framebuffer_type != 1) {
        return;
    }

    sleep_ms(5000);

    fb = (volatile uint8_t *)(uintptr_t)mbi->framebuffer_addr;
    pitch = mbi->framebuffer_pitch;
    screen_width = mbi->framebuffer_width;
    screen_height = mbi->framebuffer_height;
    bytes_per_pixel = mbi->framebuffer_bpp / 8;
    logo_bmp = (unsigned char *)_binary_kernel_video_logo_bmp_start;
    data_offset = *(uint32_t *)&logo_bmp[10];
    img_width = *(int32_t *)&logo_bmp[18];
    img_height = *(int32_t *)&logo_bmp[22];
    bpp = *(uint16_t *)&logo_bmp[28];

    if (logo_bmp[0] != 'B' || logo_bmp[1] != 'M' || bpp != 8 ||
        bytes_per_pixel == 0 || bytes_per_pixel > 4) {
        return;
    }

    pixel_data = logo_bmp + data_offset;
    palette = logo_bmp + 54;
    absolute_height = (img_height < 0) ? -img_height : img_height;
    row_stride = ((img_width * (bpp / 8) + 3) & ~3);
    start_x = ((int)screen_width - img_width) / 2;
    start_y = ((int)screen_height - absolute_height) / 2;

    for (int step = 0; step <= 32; step++) {
        for (int y = 0; y < absolute_height; y++) {
            int src_y = (img_height > 0) ? (absolute_height - 1 - y) : y;
            int screen_y = start_y + y;
            unsigned char *row_ptr = pixel_data + src_y * row_stride;

            if (screen_y < 0 || screen_y >= (int)screen_height) continue;

            for (int x = 0; x < img_width; x++) {
                uint32_t hash = (uint32_t)(x * 92837111u + y * 689287499u + step * 283923481u);
                int screen_x = start_x + x;
                volatile uint8_t *screen_pixel;

                if (screen_x < 0 || screen_x >= (int)screen_width ||
                    (hash & 0xff) >= (uint32_t)(step * 8)) {
                    continue;
                }

                screen_pixel = fb + screen_y * pitch + screen_x * bytes_per_pixel;
                for (uint32_t byte = 0; byte < bytes_per_pixel; byte++) {
                    screen_pixel[byte] = 0;
                }
            }
        }
        sleep_ms(60);
    }
}