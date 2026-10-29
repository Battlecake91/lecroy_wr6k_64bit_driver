#ifndef LECS65_DMA_EXPERIMENT_GATE_H
#define LECS65_DMA_EXPERIMENT_GATE_H

#include <stdint.h>
#include "DmaLayout.h"

typedef enum _LECS65_FIRST_MAM_GATE_RESULT {
    LecFirstMamGateAccepted = 0,
    LecFirstMamGateInvalidShape,
    LecFirstMamGateAlreadyConsumed
} LECS65_FIRST_MAM_GATE_RESULT;

typedef struct _LECS65_FIRST_MAM_SHAPE {
    uint8_t Channel;
    uint32_t Config;
    uint32_t TransferBytes;
    uint32_t SegmentCount;
    uint64_t FirstDeviceAddress;
    uint32_t FirstSegmentBytes;
    uint64_t DescriptorTableDeviceAddress;
    const LECS65_DMA_LAYOUT_ENTRY* DescriptorTable;
    uint32_t DescriptorTableBytes;
    uint32_t DescriptorEntries;
} LECS65_FIRST_MAM_SHAPE;

typedef long (*LECS65_FIRST_MAM_COMPARE_EXCHANGE)(
    volatile long* Target,
    long Exchange,
    long Comparand);

LECS65_FIRST_MAM_GATE_RESULT
LecFirstMamValidateAndConsume(
    const LECS65_FIRST_MAM_SHAPE* Shape,
    volatile long* Consumed,
    LECS65_FIRST_MAM_COMPARE_EXCHANGE CompareExchange);

#endif
