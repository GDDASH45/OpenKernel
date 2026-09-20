#include <kernel/multiboot.h>

// Symbols generated automatically by objcopy
extern char _binary_kernel_video_logo_bmp_start[];

void draw_bmp(struct multiboot_info *mbi) {
    if (!(mbi->flags & (1 << 12))) {
        return; 
    }

    volatile uint32_t *fb = (volatile uint32_t *)(uintptr_t)mbi->framebuffer_addr;
    uint32_t pitch = mbi->framebuffer_pitch;
    uint32_t screen_width = mbi->framebuffer_width;
    uint32_t screen_height = mbi->framebuffer_height;

    // Point directly to the raw binary data loaded in memory
    unsigned char *logo_bmp = (unsigned char *)_binary_kernel_video_logo_bmp_start;

    uint32_t data_offset = *(uint32_t *)&logo_bmp[10];
    int32_t img_width    = *(int32_t *)&logo_bmp[18];
    int32_t img_height   = *(int32_t *)&logo_bmp[22];
    uint16_t bpp         = *(uint16_t *)&logo_bmp[28];

    unsigned char *pixel_data = logo_bmp + data_offset;

    int start_x = (screen_width - img_width) / 2;
    int start_y = (screen_height - (img_height < 0 ? -img_height : img_height)) / 2;

    int absolute_height = (img_height < 0) ? -img_height : img_height;
    int is_bottom_up = (img_height > 0);
    int row_stride = ((img_width * (bpp / 8) + 3) & ~3);

    for (int y = 0; y < absolute_height; y++) {
        int src_y = is_bottom_up ? (absolute_height - 1 - y) : y;
        int screen_y = start_y + y;

        if (screen_y >= screen_height) continue;

        unsigned char *row_ptr = pixel_data + (src_y * row_stride);

        for (int x = 0; x < img_width; x++) {
            int screen_x = start_x + x;
            if (screen_x >= screen_width) continue;

            uint32_t color = 0;

            if (bpp == 24) {
                int src_index = x * 3;
                unsigned char b = row_ptr[src_index + 0];
                unsigned char g = row_ptr[src_index + 1];
                unsigned char r = row_ptr[src_index + 2];
                color = (r << 16) | (g << 8) | b;
            } else if (bpp == 32) {
                int src_index = x * 4;
                unsigned char b = row_ptr[src_index + 0];
                unsigned char g = row_ptr[src_index + 1];
                unsigned char r = row_ptr[src_index + 2];
                color = (0xFF << 24) | (r << 16) | (g << 8) | b;
            }

            uint32_t *screen_pixel = (uint32_t *)((uint8_t *)fb + screen_y * pitch + screen_x * 4);
            *screen_pixel = color;
        }
    }
}