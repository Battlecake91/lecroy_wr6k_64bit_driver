#include "DmaExperimentGate.h"

#define LECS65_FIRST_MAM_BYTES 0x10U
#define LECS65_FIRST_MAM_CHANNEL 0x30U
#define LECS65_FIRST_MAM_CONFIG 0x00000C00U
#define LECS65_U32_LIMIT 0xFFFFFFFFULL

LECS65_FIRST_MAM_GATE_RESULT
LecFirstMamValidateAndConsume(
    const LECS65_FIRST_MAM_SHAPE* Shape,
    volatile long* Consumed,
    LECS65_FIRST_MAM_COMPARE_EXCHANGE CompareExchange)
{
    const LECS65_DMA_LAYOUT_ENTRY* descriptors;
    uint64_t segmentEnd;
    uint64_t descriptorTableEnd;
    uint64_t payloadBytes;

    if (Shape == 0 || Consumed == 0 || CompareExchange == 0 ||
        Shape->Channel != LECS65_FIRST_MAM_CHANNEL ||
        Shape->Config != LECS65_FIRST_MAM_CONFIG ||
        Shape->TransferBytes != LECS65_FIRST_MAM_BYTES ||
        Shape->SegmentCount != 1U ||
        Shape->FirstSegmentBytes != Shape->TransferBytes ||
        Shape->DescriptorEntries != 2U ||
        Shape->DescriptorTable == 0 ||
        Shape->DescriptorTableBytes <
            2U * (uint32_t)sizeof(LECS65_DMA_LAYOUT_ENTRY) ||
        (Shape->DescriptorTableBytes % LECS65_DMA_LAYOUT_PAGE_BYTES) != 0 ||
        Shape->DescriptorTableDeviceAddress > LECS65_U32_LIMIT ||
        (Shape->DescriptorTableDeviceAddress &
            (LECS65_DMA_LAYOUT_PAGE_BYTES - 1U)) != 0 ||
        Shape->FirstDeviceAddress > LECS65_U32_LIMIT ||
        (Shape->FirstDeviceAddress & 3U) != 0) {
        return LecFirstMamGateInvalidShape;
    }

    segmentEnd = Shape->FirstDeviceAddress + Shape->FirstSegmentBytes - 1U;
    if (segmentEnd > LECS65_U32_LIMIT) {
        return LecFirstMamGateInvalidShape;
    }

    descriptorTableEnd = Shape->DescriptorTableDeviceAddress +
        Shape->DescriptorTableBytes - 1U;
    if (descriptorTableEnd > LECS65_U32_LIMIT) {
        return LecFirstMamGateInvalidShape;
    }

    descriptors = Shape->DescriptorTable;
    payloadBytes = (uint64_t)descriptors[0].CountDwords * sizeof(uint32_t);
    if (payloadBytes != Shape->TransferBytes ||
        descriptors[0].DeviceAddress != (uint32_t)Shape->FirstDeviceAddress ||
        descriptors[1].CountDwords != 0 ||
        descriptors[1].DeviceAddress != 0) {
        return LecFirstMamGateInvalidShape;
    }

    if (CompareExchange(Consumed, 1, 0) != 0) {
        return LecFirstMamGateAlreadyConsumed;
    }
    return LecFirstMamGateAccepted;
}

LECS65_MAM_LAUNCH_LIMIT_RESULT
LecMamLaunchLimitTryConsume(
    volatile long* Consumed,
    long Limit,
    LECS65_FIRST_MAM_COMPARE_EXCHANGE CompareExchange,
    long* AdmissionNumber)
{
    long current;
    long observed;

    if (Consumed == 0 || CompareExchange == 0 || AdmissionNumber == 0 ||
        Limit <= 0) {
        return LecMamLaunchLimitInvalid;
    }

    current = *Consumed;
    for (;;) {
        if (current < 0 || current >= Limit) {
            return current < 0 ?
                LecMamLaunchLimitInvalid : LecMamLaunchLimitExhausted;
        }
        observed = CompareExchange(Consumed, current + 1, current);
        if (observed < 0) {
            return LecMamLaunchLimitInvalid;
        }
        if (observed == current) {
            *AdmissionNumber = current + 1;
            return LecMamLaunchLimitAccepted;
        }
        current = observed;
    }
}
