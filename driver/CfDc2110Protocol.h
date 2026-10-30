#pragma once

/*
 * Hardware-independent validation for the legacy CFDC2110 family-2 opcode
 * 0x03 request/response pair. The helper validates framing only; it never
 * touches hardware, allocates memory or changes DMA admission.
 */
#include <stdint.h>

#define LECS65_CFDC2110_DEFAULT_RESPONSE_TIMEOUT_MS 5000U
#define LECS65_CFDC2110_OPCODE03_RESPONSE_TIMEOUT_MS 10000U
#define LECS65_CFDC2110_OPCODE03_COMMAND_PAYLOAD 112U
#define LECS65_CFDC2110_OPCODE03_RESPONSE_OUTPUT 8U
#define LECS65_CFDC2110_OPCODE03_ERROR_PAYLOAD 0x0020U

typedef enum _LECS65_CFDC2110_OPCODE03_RESULT {
    LecCfDc2110Opcode03Ok = 0,
    LecCfDc2110Opcode03Invalid = 1,
    LecCfDc2110Opcode03Truncated = 2,
    LecCfDc2110Opcode03OutputTooSmall = 3
} LECS65_CFDC2110_OPCODE03_RESULT;

typedef struct _LECS65_CFDC2110_OPCODE03_PLAN {
    uint32_t CommandSendOffset;
    uint32_t CommandSendLength;
    uint32_t ResponseRecordOffset;
    uint32_t ResponseRecordBytes;
    uint32_t ResponsePayloadCapacity;
    uint32_t ResponseTimeoutMilliseconds;
    uint32_t TotalOutputBytes;
} LECS65_CFDC2110_OPCODE03_PLAN;

typedef struct _LECS65_CFDC2110_OPCODE03_FAILURE {
    uint32_t ResponseStatus;
    uint16_t ResponsePayloadLength;
    uint16_t ResponseProtocolStatus;
} LECS65_CFDC2110_OPCODE03_FAILURE;

LECS65_CFDC2110_OPCODE03_RESULT
LecCfDc2110PlanFamily2Opcode03(
    const uint8_t* Buffer,
    uint32_t InputLength,
    uint32_t OutputLength,
    LECS65_CFDC2110_OPCODE03_PLAN* Plan);

void
LecCfDc2110MapOpcode03TransportFailure(
    uint32_t TransportStatus,
    uint32_t IoTimeoutStatus,
    uint32_t WaitTimeoutStatus,
    LECS65_CFDC2110_OPCODE03_FAILURE* Failure);
