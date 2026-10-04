#include "DmaLayout.h"

#define LECS65_U32_LIMIT 0xFFFFFFFFULL

/*
 * Slot 511 of each page is a link slot whenever another slot is needed.
 * Even the final zero descriptor must be placed on a real data slot.
 */
static LECS65_DMA_LAYOUT_RESULT
LecDmaAppendEntry(
    LECS65_DMA_LAYOUT_ENTRY* table,
    uint32_t capacity,
    uint64_t tableBase,
    uint32_t* slot,
    uint32_t count,
    uint32_t address)
{
    if ((*slot % LECS65_DMA_LAYOUT_SLOTS_PER_PAGE) ==
        LECS65_DMA_LAYOUT_SLOTS_PER_PAGE - 1U) {
        uint64_t nextPageAddress =
            tableBase +
            ((uint64_t)(*slot / LECS65_DMA_LAYOUT_SLOTS_PER_PAGE) + 1U) *
            LECS65_DMA_LAYOUT_PAGE_BYTES;

        if (*slot >= capacity || (*slot + 1U) >= capacity) {
            return LecDmaLayoutCapacity;
        }
        if (nextPageAddress > LECS65_U32_LIMIT) {
            return LecDmaLayoutAddressRange;
        }

        table[*slot].CountDwords = 0;
        table[*slot].DeviceAddress = (uint32_t)nextPageAddress;
        ++(*slot);
    }

    if (*slot >= capacity) {
        return LecDmaLayoutCapacity;
    }

    table[*slot].CountDwords = count;
    table[*slot].DeviceAddress = address;
    ++(*slot);
    return LecDmaLayoutOk;
}

LECS65_DMA_LAYOUT_RESULT
LecDmaEncodeMappedSegments(
    LECS65_DMA_LAYOUT_ENTRY* Table,
    uint32_t TableBytes,
    uint64_t TableDeviceAddress,
    const LECS65_DMA_MAPPED_SEGMENT* Segments,
    uint32_t SegmentCount,
    uint32_t ExpectedBytes,
    uint32_t* UsedEntries)
{
    uint32_t i;
    uint32_t slot = 0;
    uint64_t total = 0;
    uint32_t capacity;
    LECS65_DMA_LAYOUT_RESULT result;

    if (UsedEntries != 0) {
        *UsedEntries = 0;
    }
    if (Table == 0 || Segments == 0 || UsedEntries == 0 ||
        SegmentCount == 0 || ExpectedBytes == 0 ||
        (ExpectedBytes & 3U) != 0 ||
        TableBytes == 0 ||
        (TableBytes % LECS65_DMA_LAYOUT_PAGE_BYTES) != 0 ||
        (TableDeviceAddress & (LECS65_DMA_LAYOUT_PAGE_BYTES - 1U)) != 0) {
        return LecDmaLayoutInvalid;
    }
    if (TableDeviceAddress > LECS65_U32_LIMIT ||
        (uint64_t)TableBytes - 1U >
            LECS65_U32_LIMIT - TableDeviceAddress) {
        return LecDmaLayoutAddressRange;
    }

    capacity = TableBytes / (uint32_t)sizeof(LECS65_DMA_LAYOUT_ENTRY);

    /* Reject unmapped holes, misalignment and 32-bit PCI address overflow. */
    for (i = 0; i < SegmentCount; ++i) {
        const LECS65_DMA_MAPPED_SEGMENT* seg = &Segments[i];
        if (seg->LengthBytes == 0 || (seg->LengthBytes & 3U) != 0 ||
            (seg->DeviceAddress & 3U) != 0) {
            return LecDmaLayoutInvalid;
        }
        if (seg->DeviceAddress > LECS65_U32_LIMIT ||
            (uint64_t)seg->LengthBytes - 1U >
                LECS65_U32_LIMIT - seg->DeviceAddress) {
            return LecDmaLayoutAddressRange;
        }

        total += seg->LengthBytes;
        if (total > ExpectedBytes) {
            return LecDmaLayoutInvalid;
        }
    }
    if (total != ExpectedBytes) {
        return LecDmaLayoutInvalid;
    }

    for (i = 0; i < SegmentCount; ++i) {
        uint64_t address = Segments[i].DeviceAddress;
        uint32_t remaining = Segments[i].LengthBytes;
        while (remaining != 0) {
            uint32_t pageRemaining =
                LECS65_DMA_LAYOUT_PAGE_BYTES -
                (uint32_t)(address & (LECS65_DMA_LAYOUT_PAGE_BYTES - 1U));
            uint32_t length = remaining < pageRemaining ?
                remaining : pageRemaining;

            result = LecDmaAppendEntry(
                Table, capacity, TableDeviceAddress, &slot,
                length / sizeof(uint32_t), (uint32_t)address);
            if (result != LecDmaLayoutOk) {
                return result;
            }
            remaining -= length;
            address += length;
        }
    }

    result = LecDmaAppendEntry(
        Table, capacity, TableDeviceAddress, &slot, 0, 0);
    if (result == LecDmaLayoutOk) {
        *UsedEntries = slot;
    }
    return result;
}
