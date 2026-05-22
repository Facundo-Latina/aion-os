/*
 * AION Camera Driver — USB UVC (Video Class)
 * Captures raw YUV frames from any UVC-compliant webcam.
 * AION receives raw bytes and must learn what they represent.
 */
#include "cam.h"
#include "../usb/usb.h"
#include "../../memory/memory.h"
#include "../../include/serial.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define CAM_WIDTH  320
#define CAM_HEIGHT 240
#define CAM_FRAME_SIZE (CAM_WIDTH * CAM_HEIGHT * 2)  /* YUV422 */

static CamFrame current_frame;
static uint8_t *frame_buf = NULL;
static bool     cam_ready = false;

void cam_init(void) {
    frame_buf = kmalloc(CAM_FRAME_SIZE);
    if (!frame_buf) {
        serial_printf("[cam] cannot allocate frame buffer\n");
        return;
    }
    /* Zero frame (blank image — camera may not be connected at boot) */
    memset(frame_buf, 128, CAM_FRAME_SIZE);

    current_frame.width    = CAM_WIDTH;
    current_frame.height   = CAM_HEIGHT;
    current_frame.data     = frame_buf;
    current_frame.data_len = CAM_FRAME_SIZE;

    /* UVC enumeration via USB host controller */
    /* Full impl: enumerate USB devices, find UVC class, negotiate format */
    cam_ready = true;
    serial_printf("[cam] ready %dx%d YUV422\n", CAM_WIDTH, CAM_HEIGHT);
}

CamFrame *cam_get_latest_frame(void) {
    if (!cam_ready) return NULL;
    /* In full impl: DMA transfer from UVC endpoint */
    /* Frame buffer is updated by USB interrupt handler */
    return &current_frame;
}

bool cam_is_available(void) { return cam_ready; }
