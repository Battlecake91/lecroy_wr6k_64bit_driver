#include "CfDc2110Protocol.h"

static uint16_t
LecCfDc2110ReadU16(const uint8_t* buffer)
{
    return (uint16_t)((uint16_t)buffer[0] |
        ((uint16_t)buffer[1] << 8));
}

LECS65_CFDC2110_OPCODE03_RESULT
LecCfDc2110PlanFamily2Opcode03(
    const uint8_t* Buffer,
    uint32_t InputLength,
    uint32_t OutputLength,
    LECS65_CFDC2110_OPCODE03_PLAN* Plan)
{
    uint32_t commandPayload;
    uint32_t commandSpan;
    uint32_t responsePayload;
    uint32_t responseSpan;
    uint32_t commandOutput;
    uint32_t responseOutput;

    if (Plan != 0) {
        Plan->CommandSendOffset = 0;
        Plan->CommandSendLength = 0;
        Plan->ResponseRecordOffset = 0;
        Plan->ResponseRecordBytes = 0;
        Plan->ResponsePayloadCapacity = 0;
        Plan->ResponseTimeoutMilliseconds = 0;
        Plan->TotalOutputBytes = 0;
    }
    if (Buffer == 0 || Plan == 0) {
        return LecCfDc2110Opcode03Invalid;
    }
    if (InputLength < 18U) {
        return LecCfDc2110Opcode03Truncated;
    }

    commandOutput = LecCfDc2110ReadU16(Buffer);
    commandPayload = LecCfDc2110ReadU16(Buffer + 2);
    if (commandPayload != LECS65_CFDC2110_OPCODE03_COMMAND_PAYLOAD) {
        return LecCfDc2110Opcode03Invalid;
    }
    commandSpan = 8U + commandPayload;
    if (commandSpan < 8U || commandSpan > InputLength ||
        InputLength - commandSpan < 10U) {
        return LecCfDc2110Opcode03Truncated;
    }
    if (commandOutput != 6U ||
        LecCfDc2110ReadU16(Buffer + 4) != 3U ||
        LecCfDc2110ReadU16(Buffer + 6) != 0xA5FBU ||
        Buffer[8] != 0x40U || Buffer[9] != 2U || Buffer[10] != 3U) {
        return LecCfDc2110Opcode03Invalid;
    }

    responseOutput = LecCfDc2110ReadU16(Buffer + commandSpan);
    responsePayload = LecCfDc2110ReadU16(Buffer + commandSpan + 2U);
    responseSpan = 8U + responsePayload;
    if (responseSpan < 8U || responseSpan > InputLength - commandSpan ||
        commandSpan + responseSpan != InputLength) {
        return LecCfDc2110Opcode03Truncated;
    }
    if (responsePayload != 2U ||
        LecCfDc2110ReadU16(Buffer + commandSpan + 4U) != 3U ||
        LecCfDc2110ReadU16(Buffer + commandSpan + 6U) != 0x85FBU ||
        Buffer[commandSpan + 8U] != 0x40U ||
        Buffer[commandSpan + 9U] != 0U) {
        return LecCfDc2110Opcode03Invalid;
    }
    if (responseOutput != LECS65_CFDC2110_OPCODE03_RESPONSE_OUTPUT ||
        commandOutput > OutputLength ||
        responseOutput > OutputLength - commandOutput) {
        return LecCfDc2110Opcode03OutputTooSmall;
    }

    Plan->CommandSendOffset = 6U;
    Plan->CommandSendLength = commandPayload + 2U;
    Plan->ResponseRecordOffset = commandSpan;
    Plan->ResponseRecordBytes = responseSpan;
    Plan->ResponsePayloadCapacity = responseOutput - 6U;
    Plan->ResponseTimeoutMilliseconds =
        LECS65_CFDC2110_OPCODE03_RESPONSE_TIMEOUT_MS;
    Plan->TotalOutputBytes = commandOutput + responseOutput;
    return LecCfDc2110Opcode03Ok;
}

void
LecCfDc2110MapOpcode03TransportFailure(
    uint32_t TransportStatus,
    uint32_t IoTimeoutStatus,
    uint32_t WaitTimeoutStatus,
    LECS65_CFDC2110_OPCODE03_FAILURE* Failure)
{
    if (Failure == 0) {
        return;
    }

    Failure->ResponseStatus =
        TransportStatus == IoTimeoutStatus
            ? WaitTimeoutStatus
            : TransportStatus;
    Failure->ResponsePayloadLength = 2U;
    Failure->ResponseProtocolStatus =
        LECS65_CFDC2110_OPCODE03_ERROR_PAYLOAD;
}
