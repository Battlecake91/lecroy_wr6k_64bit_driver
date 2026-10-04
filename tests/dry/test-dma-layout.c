/*
 * Portable software-only tests for the descriptor encoder.
 * These are synthetic DMA logical addresses, not CPU PFNs or PCI accesses.
 */
#include <stdint.h>
#include <stdio.h>
#include "../../driver/DmaLayout.h"

static LECS65_DMA_LAYOUT_ENTRY table[1024];
static LECS65_DMA_MAPPED_SEGMENT segments[512];
static unsigned passed;
static unsigned failed;

#define CHECK(name, cond) do {                                       \
    if (cond) { ++passed; printf("[PASS] %s\\n", name); }             \
    else { ++failed; printf("[FAIL] %s\\n", name); }                   \
} while (0)

int main(void)
{
    uint32_t used = 0;
    uint32_t i;
    LECS65_DMA_LAYOUT_RESULT rc;

    segments[0].DeviceAddress = 0x00123000ULL;
    segments[0].LengthBytes = 0x2000U;
    rc = LecDmaEncodeMappedSegments(
        table, 8192U, 0x00200000ULL, segments, 1, 8192, &used);
    CHECK("8-KiB contiguous segment splits at page boundary",
          rc == LecDmaLayoutOk && used == 3 &&
          table[0].DeviceAddress == 0x00123000U &&
          table[0].CountDwords == 1024U &&
          table[1].DeviceAddress == 0x00124000U &&
          table[1].CountDwords == 1024U &&
          table[2].CountDwords == 0);

    segments[0].DeviceAddress = 0x00001FFCU;
    segments[0].LengthBytes = 8;
    rc = LecDmaEncodeMappedSegments(
        table, 8192U, 0x00200000ULL, segments, 1, 8, &used);
    CHECK("unaligned page offset creates two aligned fragments",
          rc == LecDmaLayoutOk && used == 3 &&
          table[0].CountDwords == 1 &&
          table[1].DeviceAddress == 0x00002000U &&
          table[1].CountDwords == 1);

    for (i = 0; i < 512; ++i) {
        segments[i].DeviceAddress = 0x00300000ULL + 4ULL * i;
        segments[i].LengthBytes = 4U;
    }

    rc = LecDmaEncodeMappedSegments(
        table, 4096U, 0x00200000ULL, segments, 510, 2040, &used);
    CHECK("510 data slots plus terminator fit one page",
          rc == LecDmaLayoutOk && used == 511 &&
          table[510].CountDwords == 0 &&
          table[510].DeviceAddress == 0);

    rc = LecDmaEncodeMappedSegments(
        table, 8192U, 0x00200000ULL, segments, 511, 2044, &used);
    CHECK("slot 511 chains and terminator occupies next page",
          rc == LecDmaLayoutOk && used == 513 &&
          table[510].CountDwords == 1 &&
          table[511].CountDwords == 0 &&
          table[511].DeviceAddress == 0x00201000U &&
          table[512].CountDwords == 0 &&
          table[512].DeviceAddress == 0);

    rc = LecDmaEncodeMappedSegments(
        table, 4096U, 0x00200000ULL, segments, 511, 2044, &used);
    CHECK("no room to chain after 511 slots",
          rc == LecDmaLayoutCapacity && used == 0);

    rc = LecDmaEncodeMappedSegments(
        table, 8192U, 0x00200000ULL, segments, 512, 2048, &used);
    CHECK("data resumes at first slot of chained page",
          rc == LecDmaLayoutOk && used == 514 &&
          table[512].CountDwords == 1 &&
          table[513].CountDwords == 0);

    rc = LecDmaEncodeMappedSegments(
        table, 8192U, 0x00200000ULL, segments, 1, 16, &used);
    CHECK("total mapping size must match request",
          rc == LecDmaLayoutInvalid && used == 0);

    segments[0].DeviceAddress = 0x100000000ULL;
    rc = LecDmaEncodeMappedSegments(
        table, 8192U, 0x00200000ULL, segments, 1, 4, &used);
    CHECK("reject out-of-range device address, not a CPU PFN",
          rc == LecDmaLayoutAddressRange && used == 0);

    segments[0].DeviceAddress = 0xFFFFFFFCULL;
    segments[0].LengthBytes = 8;
    rc = LecDmaEncodeMappedSegments(
        table, 8192U, 0x00200000ULL, segments, 1, 8, &used);
    CHECK("reject segment crossing 4GiB boundary",
          rc == LecDmaLayoutAddressRange);

    segments[0].DeviceAddress = 0x00300000ULL;
    segments[0].LengthBytes = 4;
    rc = LecDmaEncodeMappedSegments(
        table, 8192U, 0xFFFFFFF0ULL, segments, 1, 4, &used);
    CHECK("reject unaligned common-buffer address",
          rc == LecDmaLayoutInvalid);
    rc = LecDmaEncodeMappedSegments(
        table, 8192U, 0xFFFFF000ULL, segments, 1, 4, &used);
    CHECK("reject common-buffer range crossing 4GiB",
          rc == LecDmaLayoutAddressRange);

    segments[0].DeviceAddress = 0x00300002ULL;
    rc = LecDmaEncodeMappedSegments(
        table, 8192U, 0x00200000ULL, segments, 1, 4, &used);
    CHECK("reject dword-misaligned segment",
          rc == LecDmaLayoutInvalid);

    segments[0].DeviceAddress = 0x00300000ULL;
    segments[0].LengthBytes = 0;
    rc = LecDmaEncodeMappedSegments(
        table, 8192U, 0x00200000ULL, segments, 1, 4, &used);
    CHECK("reject zero-length segment",
          rc == LecDmaLayoutInvalid);

    printf("DMA LAYOUT: %u/%u passed; %u failed.\\n",
        passed, passed + failed, failed);
    return failed == 0 ? 0 : 1;
}
