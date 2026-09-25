#pragma once

/*
 * Device interface classes registered by the original LecS65AcqDrv.sys.
 *
 * This file intentionally stores the GUID components without depending on
 * INITGUID/DEFINE_GUID so it can be consumed by both analysis/test utilities
 * and the future kernel project.
 */

#include <stdint.h>

typedef struct LECS65_GUID_VALUE {
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t  data4[8];
} LECS65_GUID_VALUE;

static const LECS65_GUID_VALUE LECS65_INTERFACE_GUID_1 = {
    UINT32_C(0x7AC34BE9), UINT16_C(0xF766), UINT16_C(0x4F15),
    { 0x9E, 0x88, 0x85, 0x4B, 0xA5, 0xE2, 0x14, 0x6E }
};

static const LECS65_GUID_VALUE LECS65_INTERFACE_GUID_2 = {
    UINT32_C(0x8D1103B8), UINT16_C(0x5BF4), UINT16_C(0x4B5C),
    { 0xB2, 0x1E, 0xEE, 0xAA, 0xCE, 0x97, 0xD4, 0x18 }
};

static const LECS65_GUID_VALUE LECS65_INTERFACE_GUID_3 = {
    UINT32_C(0x9007C2BC), UINT16_C(0xEDFD), UINT16_C(0x4F2F),
    { 0xA0, 0x59, 0xDF, 0x11, 0x31, 0xCB, 0x1A, 0xE5 }
};

static const LECS65_GUID_VALUE LECS65_INTERFACE_GUID_4 = {
    UINT32_C(0xFC5DF040), UINT16_C(0xD6CD), UINT16_C(0x4BA0),
    { 0xB5, 0xE0, 0x25, 0x61, 0x97, 0x29, 0x63, 0xA2 }
};
