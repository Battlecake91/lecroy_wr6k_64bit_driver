#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include "../../driver/DmaExperimentGate.h"

static unsigned passed;
static unsigned failed;

static void Check(const char* name, int ok)
{
    printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
    if (ok) ++passed; else ++failed;
}

static long CompareExchange(
    volatile long* target,
    long exchange,
    long comparand)
{
    return InterlockedCompareExchange(
        (volatile LONG*)target, (LONG)exchange, (LONG)comparand);
}

static LECS65_FIRST_MAM_SHAPE ValidShape(
    LECS65_DMA_LAYOUT_ENTRY* descriptors)
{
    LECS65_FIRST_MAM_SHAPE shape;
    ZeroMemory(&shape, sizeof(shape));
    descriptors[0].CountDwords = 4;
    descriptors[0].DeviceAddress = 0x00123000U;
    descriptors[1].CountDwords = 0;
    descriptors[1].DeviceAddress = 0;
    shape.Channel = 0x30;
    shape.Config = 0x00000C00U;
    shape.TransferBytes = 16;
    shape.SegmentCount = 1;
    shape.FirstDeviceAddress = 0x00123000ULL;
    shape.FirstSegmentBytes = 16;
    shape.DescriptorTableDeviceAddress = 0x00200000ULL;
    shape.DescriptorTable = descriptors;
    shape.DescriptorTableBytes = 4096;
    shape.DescriptorEntries = 2;
    return shape;
}

typedef struct _THREAD_CONTEXT {
    HANDLE StartEvent;
    const LECS65_FIRST_MAM_SHAPE* Shape;
    volatile long* Latch;
    LECS65_FIRST_MAM_GATE_RESULT Result;
} THREAD_CONTEXT;

typedef struct _LIMIT_THREAD_CONTEXT {
    HANDLE StartEvent;
    volatile long* Counter;
    long Limit;
    LECS65_MAM_LAUNCH_LIMIT_RESULT Result;
    long AdmissionNumber;
} LIMIT_THREAD_CONTEXT;

static DWORD WINAPI GateThread(void* parameter)
{
    THREAD_CONTEXT* context = (THREAD_CONTEXT*)parameter;
    WaitForSingleObject(context->StartEvent, INFINITE);
    context->Result = LecFirstMamValidateAndConsume(
        context->Shape, context->Latch, CompareExchange);
    return 0;
}

static DWORD WINAPI LimitThread(void* parameter)
{
    LIMIT_THREAD_CONTEXT* context = (LIMIT_THREAD_CONTEXT*)parameter;
    WaitForSingleObject(context->StartEvent, INFINITE);
    context->Result = LecMamLaunchLimitTryConsume(
        context->Counter,
        context->Limit,
        CompareExchange,
        &context->AdmissionNumber);
    return 0;
}

int main(void)
{
    LECS65_DMA_LAYOUT_ENTRY descriptors[2];
    LECS65_FIRST_MAM_SHAPE shape;
    LECS65_FIRST_MAM_SHAPE invalid;
    volatile long latch;
    LECS65_FIRST_MAM_GATE_RESULT result;
    THREAD_CONTEXT contexts[2];
    HANDLE threads[2];
    HANDLE startEvent;
    unsigned accepted;
    unsigned busy;
    long admissionNumber;
    LIMIT_THREAD_CONTEXT limitContexts[2];

    shape = ValidShape(descriptors);
    latch = 0;
    result = LecFirstMamValidateAndConsume(&shape, &latch, CompareExchange);
    Check("exact 16-byte single-segment shape is accepted",
        result == LecFirstMamGateAccepted && latch == 1);
    Check("repeated GO attempt is rejected",
        LecFirstMamValidateAndConsume(&shape, &latch, CompareExchange) ==
            LecFirstMamGateAlreadyConsumed && latch == 1);

    invalid = shape;
    invalid.Channel = 0x31;
    latch = 0;
    Check("wrong channel is rejected before latch consumption",
        LecFirstMamValidateAndConsume(&invalid, &latch, CompareExchange) ==
            LecFirstMamGateInvalidShape && latch == 0);
    invalid = shape;
    invalid.Config = 0x00000C01U;
    Check("wrong config is rejected",
        LecFirstMamValidateAndConsume(&invalid, &latch, CompareExchange) ==
            LecFirstMamGateInvalidShape && latch == 0);
    invalid = shape;
    invalid.TransferBytes = 20;
    Check("wrong transfer length is rejected",
        LecFirstMamValidateAndConsume(&invalid, &latch, CompareExchange) ==
            LecFirstMamGateInvalidShape && latch == 0);
    invalid = shape;
    invalid.SegmentCount = 2;
    Check("more than one SG segment is rejected",
        LecFirstMamValidateAndConsume(&invalid, &latch, CompareExchange) ==
            LecFirstMamGateInvalidShape && latch == 0);
    invalid = shape;
    invalid.FirstSegmentBytes = 12;
    Check("partial mapped segment is rejected",
        LecFirstMamValidateAndConsume(&invalid, &latch, CompareExchange) ==
            LecFirstMamGateInvalidShape && latch == 0);
    invalid = shape;
    invalid.FirstDeviceAddress = 0xFFFFFFFCULL;
    Check("DMA logical range crossing 4GiB is rejected",
        LecFirstMamValidateAndConsume(&invalid, &latch, CompareExchange) ==
            LecFirstMamGateInvalidShape && latch == 0);
    invalid = shape;
    invalid.DescriptorTableDeviceAddress = 0x100000000ULL;
    Check("out-of-range descriptor table is rejected",
        LecFirstMamValidateAndConsume(&invalid, &latch, CompareExchange) ==
            LecFirstMamGateInvalidShape && latch == 0);
    invalid = shape;
    invalid.DescriptorTableDeviceAddress = 0x00200004ULL;
    Check("misaligned descriptor table is rejected",
        LecFirstMamValidateAndConsume(&invalid, &latch, CompareExchange) ==
            LecFirstMamGateInvalidShape && latch == 0);
    invalid = shape;
    invalid.DescriptorTableDeviceAddress = 0xFFFFF000ULL;
    invalid.DescriptorTableBytes = 8192;
    Check("descriptor table range crossing 4GiB is rejected",
        LecFirstMamValidateAndConsume(&invalid, &latch, CompareExchange) ==
            LecFirstMamGateInvalidShape && latch == 0);
    invalid = shape;
    invalid.DescriptorTableBytes = 4097;
    Check("partial descriptor-table allocation is rejected",
        LecFirstMamValidateAndConsume(&invalid, &latch, CompareExchange) ==
            LecFirstMamGateInvalidShape && latch == 0);
    invalid = shape;
    descriptors[0].CountDwords = 3;
    Check("descriptor payload mismatch is rejected",
        LecFirstMamValidateAndConsume(&invalid, &latch, CompareExchange) ==
            LecFirstMamGateInvalidShape && latch == 0);
    shape = ValidShape(descriptors);
    descriptors[0].DeviceAddress++;
    Check("descriptor address mismatch is rejected",
        LecFirstMamValidateAndConsume(&shape, &latch, CompareExchange) ==
            LecFirstMamGateInvalidShape && latch == 0);
    shape = ValidShape(descriptors);
    descriptors[1].DeviceAddress = 1;
    Check("invalid descriptor terminator is rejected",
        LecFirstMamValidateAndConsume(&shape, &latch, CompareExchange) ==
            LecFirstMamGateInvalidShape && latch == 0);

    shape = ValidShape(descriptors);
    latch = 0;
    invalid = shape;
    invalid.FirstSegmentBytes = 8;
    Check("failed preparation does not consume the launch attempt",
        LecFirstMamValidateAndConsume(&invalid, &latch, CompareExchange) ==
            LecFirstMamGateInvalidShape &&
        LecFirstMamValidateAndConsume(&shape, &latch, CompareExchange) ==
            LecFirstMamGateAccepted && latch == 1);

    latch = 0;
    startEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    ZeroMemory(contexts, sizeof(contexts));
    contexts[0].StartEvent = startEvent;
    contexts[1].StartEvent = startEvent;
    contexts[0].Shape = &shape;
    contexts[1].Shape = &shape;
    contexts[0].Latch = &latch;
    contexts[1].Latch = &latch;
    threads[0] = CreateThread(NULL, 0, GateThread, &contexts[0], 0, NULL);
    threads[1] = CreateThread(NULL, 0, GateThread, &contexts[1], 0, NULL);
    SetEvent(startEvent);
    WaitForMultipleObjects(2, threads, TRUE, INFINITE);
    accepted = (contexts[0].Result == LecFirstMamGateAccepted) +
        (contexts[1].Result == LecFirstMamGateAccepted);
    busy = (contexts[0].Result == LecFirstMamGateAlreadyConsumed) +
        (contexts[1].Result == LecFirstMamGateAlreadyConsumed);
    Check("concurrent handles admit exactly one attempt",
        accepted == 1 && busy == 1 && latch == 1);
    CloseHandle(threads[0]);
    CloseHandle(threads[1]);
    CloseHandle(startEvent);

    latch = 0;
    admissionNumber = 0;
    Check("regular MAM launch limit admits the first request",
        LecMamLaunchLimitTryConsume(
            &latch, 1, CompareExchange, &admissionNumber) ==
            LecMamLaunchLimitAccepted &&
        admissionNumber == 1 && latch == 1);
    admissionNumber = 0;
    Check("regular MAM launch limit rejects a repeated request",
        LecMamLaunchLimitTryConsume(
            &latch, 1, CompareExchange, &admissionNumber) ==
            LecMamLaunchLimitExhausted &&
        admissionNumber == 0 && latch == 1);
    latch = 0;
    Check("zero regular MAM launch limit is invalid",
        LecMamLaunchLimitTryConsume(
            &latch, 0, CompareExchange, &admissionNumber) ==
            LecMamLaunchLimitInvalid && latch == 0);

    latch = 0;
    startEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    ZeroMemory(limitContexts, sizeof(limitContexts));
    limitContexts[0].StartEvent = startEvent;
    limitContexts[1].StartEvent = startEvent;
    limitContexts[0].Counter = &latch;
    limitContexts[1].Counter = &latch;
    limitContexts[0].Limit = 1;
    limitContexts[1].Limit = 1;
    threads[0] = CreateThread(NULL, 0, LimitThread, &limitContexts[0], 0, NULL);
    threads[1] = CreateThread(NULL, 0, LimitThread, &limitContexts[1], 0, NULL);
    SetEvent(startEvent);
    WaitForMultipleObjects(2, threads, TRUE, INFINITE);
    accepted = (limitContexts[0].Result == LecMamLaunchLimitAccepted) +
        (limitContexts[1].Result == LecMamLaunchLimitAccepted);
    busy = (limitContexts[0].Result == LecMamLaunchLimitExhausted) +
        (limitContexts[1].Result == LecMamLaunchLimitExhausted);
    Check("regular MAM limit admits one concurrent request",
        accepted == 1 && busy == 1 && latch == 1);
    CloseHandle(threads[0]);
    CloseHandle(threads[1]);
    CloseHandle(startEvent);

    printf("FIRST MAM GATE: %u/%u passed; %u failed.\n",
        passed, passed + failed, failed);
    return failed == 0 ? 0 : 1;
}
