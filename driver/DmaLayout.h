#pragma once
/*
 * Hardware-independent encoder for the WR6k SG descriptor ABI.
 *
 * Every address passed to this API MUST already be a device-visible DMA
 * logical address from the Windows DMA adapter. CPU PFNs are not acceptable.
 * TableAddress must be the device-logical base of a contiguous common buffer
 * with page-aligned virtual and logical bases (one 4 KiB page per 512 slots).
 *
 * This helper does not acquire/release DMA mappings or start the hardware.
 */
#include <stdint.h>

#define LECS65_DMA_LAYOUT_PAGE_BYTES 4096U
#define LECS65_DMA_LAYOUT_SLOTS_PER_PAGE 512U

typedef struct _LECS65_DMA_MAPPED_SEGMENT {
    uint64_t DeviceAddress;
    uint32_t LengthBytes;
} LECS65_DMA_MAPPED_SEGMENT;

typedef struct _LECS65_DMA_LAYOUT_ENTRY {
    uint32_t CountDwords;
    uint32_t DeviceAddress;
} LECS65_DMA_LAYOUT_ENTRY;

typedef enum _LECS65_DMA_LAYOUT_RESULT {
    LecDmaLayoutOk = 0,
    LecDmaLayoutInvalid = 1,
    LecDmaLayoutAddressRange = 2,
    LecDmaLayoutCapacity = 3
} LECS65_DMA_LAYOUT_RESULT;

LECS65_DMA_LAYOUT_RESULT
LecDmaEncodeMappedSegments(
    LECS65_DMA_LAYOUT_ENTRY* Table,
    uint32_t TableBytes,
    uint64_t TableDeviceAddress,
    const LECS65_DMA_MAPPED_SEGMENT* Segments,
    uint32_t SegmentCount,
    uint32_t ExpectedBytes,
    uint32_t* UsedEntries);
