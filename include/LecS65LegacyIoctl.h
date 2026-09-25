#pragma once

/*
 * Reconstructed legacy ABI for LecS65AcqDrv.sys.
 *
 * Source: static analysis of LeCroy S65 Acquisition Driver 6.1.1.1002.
 * These definitions describe the externally visible binary interface.
 * Unknown controls intentionally keep numeric names until their semantics
 * are proven.
 */

#include <stdint.h>

/* LeCroy / S65 controls */
#define LECS65_IOCTL_00222C00                    UINT32_C(0x00222C00)
#define LECS65_IOCTL_00222C04                    UINT32_C(0x00222C04)
#define LECS65_IOCTL_00223000                    UINT32_C(0x00223000)
#define LECS65_IOCTL_00223004                    UINT32_C(0x00223004)
#define LECS65_IOCTL_0022303C                    UINT32_C(0x0022303C)
#define LECS65_IOCTL_00223040                    UINT32_C(0x00223040)
#define LECS65_IOCTL_00223044                    UINT32_C(0x00223044)

#define LECS65_IOCTL_GET_DALLAS_ID                UINT32_C(0x00223080)
#define LECS65_IOCTL_READ_DALLAS_MEMORY           UINT32_C(0x00223084)
#define LECS65_IOCTL_WRITE_DALLAS_MEMORY          UINT32_C(0x00223088)

#define LECS65_IOCTL_00223100                    UINT32_C(0x00223100)

/* DriverWorks / Aladdin-family controls used by this driver */
#define LECS65_IOCTL_CFDC2110                    UINT32_C(0xCFDC2110)
#define LECS65_IOCTL_CFDC2124                    UINT32_C(0xCFDC2124)
#define LECS65_IOCTL_CFDC2128                    UINT32_C(0xCFDC2128)
#define LECS65_IOCTL_CFDC212C                    UINT32_C(0xCFDC212C)
#define LECS65_IOCTL_PROG_SERTRIG_FPGA            UINT32_C(0xCFDC2130)
#define LECS65_IOCTL_CFDC2138                    UINT32_C(0xCFDC2138)
#define LECS65_IOCTL_CFDC2180                    UINT32_C(0xCFDC2180)
#define LECS65_IOCTL_CFDC2184                    UINT32_C(0xCFDC2184)
#define LECS65_IOCTL_CFDC218C                    UINT32_C(0xCFDC218C)
#define LECS65_IOCTL_CFDC2190                    UINT32_C(0xCFDC2190)
#define LECS65_IOCTL_CFDC2194                    UINT32_C(0xCFDC2194)

#define LECS65_IOCTL_REGISTER_READ               UINT32_C(0xCFDC21C0)
#define LECS65_IOCTL_REGISTER_WRITE              UINT32_C(0xCFDC21C4)
#define LECS65_IOCTL_GET_DRIVER_BUILD            UINT32_C(0xCFDC21C8)

#define LECS65_IOCTL_CFDC2400                    UINT32_C(0xCFDC2400)

/* METHOD_NEITHER transfer path: x86/x64 compatibility-sensitive. */
#define LECS65_IOCTL_CFDD219F                    UINT32_C(0xCFDD219F)

#define LECS65_LEGACY_DRIVER_BUILD               UINT32_C(1002)

#pragma pack(push, 1)

/* 0xCFDC21C0, 4-byte form: BAR0 implied. */
typedef struct LECS65_REG_READ_LEGACY {
    uint32_t offset;
} LECS65_REG_READ_LEGACY;

/* 0xCFDC21C0, 5-byte form. */
typedef struct LECS65_REG_READ_EXT {
    uint8_t  bar;
    uint32_t offset;
} LECS65_REG_READ_EXT;

/* 0xCFDC21C4, 8-byte form: BAR0 implied. */
typedef struct LECS65_REG_WRITE_LEGACY {
    uint32_t offset;
    uint32_t value;
} LECS65_REG_WRITE_LEGACY;

/* 0xCFDC21C4, 9-byte form. */
typedef struct LECS65_REG_WRITE_EXT {
    uint8_t  bar;
    uint32_t offset;
    uint32_t value;
} LECS65_REG_WRITE_EXT;

#pragma pack(pop)

#if defined(__cplusplus)
static_assert(sizeof(LECS65_REG_READ_LEGACY) == 4, "legacy register-read ABI changed");
static_assert(sizeof(LECS65_REG_READ_EXT) == 5, "extended register-read ABI changed");
static_assert(sizeof(LECS65_REG_WRITE_LEGACY) == 8, "legacy register-write ABI changed");
static_assert(sizeof(LECS65_REG_WRITE_EXT) == 9, "extended register-write ABI changed");
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(LECS65_REG_READ_LEGACY) == 4, "legacy register-read ABI changed");
_Static_assert(sizeof(LECS65_REG_READ_EXT) == 5, "extended register-read ABI changed");
_Static_assert(sizeof(LECS65_REG_WRITE_LEGACY) == 8, "legacy register-write ABI changed");
_Static_assert(sizeof(LECS65_REG_WRITE_EXT) == 9, "extended register-write ABI changed");
#endif
