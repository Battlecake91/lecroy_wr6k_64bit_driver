#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../driver/CfDc2110Protocol.h"

static unsigned passed;
static unsigned failed;

static void Check(const char* name, int ok)
{
    printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
    if (ok) ++passed; else ++failed;
}

static void WriteU16(uint8_t* target, uint16_t value)
{
    target[0] = (uint8_t)value;
    target[1] = (uint8_t)(value >> 8);
}

static void BuildObservedShape(uint8_t buffer[130])
{
    unsigned i;

    memset(buffer, 0, 130);
    WriteU16(buffer + 0, 6);
    WriteU16(buffer + 2, 112);
    WriteU16(buffer + 4, 3);
    WriteU16(buffer + 6, 0xA5FB);
    buffer[8] = 0x40;
    buffer[9] = 2;
    buffer[10] = 3;
    buffer[11] = 7;
    for (i = 12; i < 120; ++i) {
        buffer[i] = (uint8_t)(i * 37U + 11U);
    }
    WriteU16(buffer + 120, 8);
    WriteU16(buffer + 122, 2);
    WriteU16(buffer + 124, 3);
    WriteU16(buffer + 126, 0x85FB);
    buffer[128] = 0x40;
    buffer[129] = 0;
}

typedef struct _THREAD_CONTEXT {
    HANDLE StartEvent;
    const uint8_t* Buffer;
    LECS65_CFDC2110_OPCODE03_RESULT Result;
    LECS65_CFDC2110_OPCODE03_PLAN Plan;
} THREAD_CONTEXT;

static DWORD WINAPI ParseThread(void* parameter)
{
    THREAD_CONTEXT* context = (THREAD_CONTEXT*)parameter;
    WaitForSingleObject(context->StartEvent, INFINITE);
    context->Result = LecCfDc2110PlanFamily2Opcode03(
        context->Buffer, 130, 14, &context->Plan);
    return 0;
}

int main(void)
{
    uint8_t request[130];
    uint8_t changed[130];
    LECS65_CFDC2110_OPCODE03_PLAN plan;
    LECS65_CFDC2110_OPCODE03_FAILURE failure;
    unsigned i;
    HANDLE startEvent;
    HANDLE threads[2];
    THREAD_CONTEXT contexts[2];

    BuildObservedShape(request);
    for (i = 0; i < 6; ++i) {
        Check("each of six observed request forms is admitted",
            LecCfDc2110PlanFamily2Opcode03(
                request, sizeof(request), 14, &plan) ==
                LecCfDc2110Opcode03Ok);
    }
    Check("exact dispatcher and transport plan is recovered",
        plan.CommandSendOffset == 6 &&
        plan.CommandSendLength == 114 &&
        plan.ResponseRecordOffset == 120 &&
        plan.ResponseRecordBytes == 10 &&
        plan.ResponsePayloadCapacity == 2 &&
        plan.ResponseTimeoutMilliseconds == 10000 &&
        plan.TotalOutputBytes == 14);
    Check("caller cannot select a response timeout",
        LECS65_CFDC2110_DEFAULT_RESPONSE_TIMEOUT_MS == 5000 &&
        LECS65_CFDC2110_OPCODE03_RESPONSE_TIMEOUT_MS == 10000);

    memcpy(changed, request, sizeof(changed));
    changed[119] ^= 0xFF;
    Check("opaque command body remains opaque",
        LecCfDc2110PlanFamily2Opcode03(
            changed, sizeof(changed), 14, &plan) == LecCfDc2110Opcode03Ok);
    Check("truncated input is rejected",
        LecCfDc2110PlanFamily2Opcode03(
            request, 129, 14, &plan) == LecCfDc2110Opcode03Truncated);
    Check("insufficient output is rejected",
        LecCfDc2110PlanFamily2Opcode03(
            request, 130, 13, &plan) == LecCfDc2110Opcode03OutputTooSmall);

    memcpy(changed, request, sizeof(changed));
    WriteU16(changed + 2, 110);
    Check("shorter command payload is rejected",
        LecCfDc2110PlanFamily2Opcode03(
            changed, sizeof(changed), 14, &plan) == LecCfDc2110Opcode03Invalid);
    memcpy(changed, request, sizeof(changed));
    WriteU16(changed + 2, 114);
    Check("longer command payload is rejected",
        LecCfDc2110PlanFamily2Opcode03(
            changed, sizeof(changed), 14, &plan) == LecCfDc2110Opcode03Invalid);
    memcpy(changed, request, sizeof(changed));
    changed[10] = 4;
    Check("wrong family-2 opcode is rejected",
        LecCfDc2110PlanFamily2Opcode03(
            changed, sizeof(changed), 14, &plan) == LecCfDc2110Opcode03Invalid);
    memcpy(changed, request, sizeof(changed));
    changed[128] = 0x41;
    Check("wrong companion fetch is rejected",
        LecCfDc2110PlanFamily2Opcode03(
            changed, sizeof(changed), 14, &plan) == LecCfDc2110Opcode03Invalid);
    memcpy(changed, request, sizeof(changed));
    WriteU16(changed + 120, 10);
    Check("unobserved response capacity is rejected",
        LecCfDc2110PlanFamily2Opcode03(
            changed, sizeof(changed), 16, &plan) == LecCfDc2110Opcode03OutputTooSmall);

    LecCfDc2110MapOpcode03TransportFailure(
        0xC00000B5U, 0xC00000B5U, 0x00000102U, &failure);
    Check("transport timeout maps to legacy wait timeout framing",
        failure.ResponseStatus == 0x00000102U &&
        failure.ResponsePayloadLength == 2 &&
        failure.ResponseProtocolStatus == 0x20);
    LecCfDc2110MapOpcode03TransportFailure(
        0xC0000001U, 0xC00000B5U, 0x00000102U, &failure);
    Check("non-timeout transport failure is preserved",
        failure.ResponseStatus == 0xC0000001U &&
        failure.ResponsePayloadLength == 2 &&
        failure.ResponseProtocolStatus == 0x20);
    LecCfDc2110MapOpcode03TransportFailure(
        0xC0000120U, 0xC00000B5U, 0x00000102U, &failure);
    Check("cancellation status is preserved in protocol response",
        failure.ResponseStatus == 0xC0000120U);

    startEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    ZeroMemory(contexts, sizeof(contexts));
    contexts[0].StartEvent = startEvent;
    contexts[1].StartEvent = startEvent;
    contexts[0].Buffer = request;
    contexts[1].Buffer = request;
    threads[0] = CreateThread(NULL, 0, ParseThread, &contexts[0], 0, NULL);
    threads[1] = CreateThread(NULL, 0, ParseThread, &contexts[1], 0, NULL);
    SetEvent(startEvent);
    WaitForMultipleObjects(2, threads, TRUE, INFINITE);
    Check("concurrent validation is deterministic and stateless",
        contexts[0].Result == LecCfDc2110Opcode03Ok &&
        contexts[1].Result == LecCfDc2110Opcode03Ok &&
        contexts[0].Plan.CommandSendLength == 114 &&
        contexts[1].Plan.CommandSendLength == 114);
    CloseHandle(threads[0]);
    CloseHandle(threads[1]);
    CloseHandle(startEvent);

    printf("CFDC2110 OPCODE03: %u/%u passed; %u failed.\n",
        passed, passed + failed, failed);
    return failed == 0 ? 0 : 1;
}
