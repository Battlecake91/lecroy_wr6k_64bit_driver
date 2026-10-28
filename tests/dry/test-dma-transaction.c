#include "../../driver/DmaTransaction.h"
#include <stdio.h>
#include <string.h>

static unsigned checks;
static unsigned barrier_calls;
static unsigned commit_calls;
#define CHECK(x) do { checks++; if (!(x)) { \
    printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

static void Barrier(void* context)
{
    unsigned* value = (unsigned*)context;
    (*value)++;
    barrier_calls++;
}

static unsigned CommitGo(void* context, uint64_t generation)
{
    uint64_t* expected = (uint64_t*)context;
    commit_calls++;
    return expected != 0 && generation == *expected;
}

static unsigned FailCommit(void* context, uint64_t generation)
{
    (void)context;
    (void)generation;
    commit_calls++;
    return 0;
}

static int PrepareMapped(LECS65_DMA_TRANSACTION* tx, uint64_t* generation)
{
    LecDmaTransactionInitialize(tx);
    CHECK(LecDmaTransactionSetLifecycle(tx, 1, 1) == LecDmaTxOk);
    CHECK(LecDmaTransactionBegin(tx, 0x11, 0x22, 4096, generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionBindOwner(tx, *generation, 0x11, 0x22) == LecDmaTxOk);
    CHECK(LecDmaTransactionAdmitAdapter(tx, *generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionAcquireBuffer(tx, *generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionBeginMapping(tx, *generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionMappingComplete(tx, *generation, 0x44, 1) == LecDmaTxOk);
    return 0;
}

static int PreparePublished(LECS65_DMA_TRANSACTION* tx, uint64_t* generation)
{
    LECS65_DMA_LAYOUT_ENTRY table[512];
    LECS65_DMA_MAPPED_SEGMENT segment = { 0x00100000, 4096 };
    unsigned localBarrier = 0;
    if (PrepareMapped(tx, generation)) return 1;
    memset(table, 0xcc, sizeof(table));
    CHECK(LecDmaTransactionBuildDescriptors(tx, *generation, table,
        sizeof(table), 0x00200000, &segment, 1) == LecDmaTxOk);
    CHECK(table[0].CountDwords == 1024);
    CHECK(table[0].DeviceAddress == 0x00100000);
    CHECK(LecDmaTransactionPublish(tx, *generation, Barrier, &localBarrier) == LecDmaTxOk);
    CHECK(localBarrier == 1);
    return 0;
}

static int TestAdmissionAndRollback(void)
{
    LECS65_DMA_TRANSACTION tx;
    uint64_t generation = 0;
    LecDmaTransactionInitialize(&tx);
    CHECK(LecDmaTransactionBegin(&tx, 1, 2, 4, &generation) == LecDmaTxNotReady);
    CHECK(LecDmaTransactionSetLifecycle(&tx, 1, 1) == LecDmaTxOk);
    CHECK(LecDmaTransactionBegin(&tx, 1, 2, 0, &generation) == LecDmaTxInvalid);
    CHECK(LecDmaTransactionBegin(&tx, 1, 2, 4, &generation) == LecDmaTxOk);
    CHECK(generation == 1);
    CHECK(LecDmaTransactionBindOwner(&tx, generation, 9, 2) == LecDmaTxWrongOwner);
    CHECK(LecDmaTransactionBindOwner(&tx, generation, 1, 2) == LecDmaTxOk);
    CHECK(LecDmaTransactionAdmitAdapter(&tx, generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionAcquireBuffer(&tx, generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionBeginMapping(&tx, generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionMappingComplete(&tx, generation, 0, 0) == LecDmaTxOk);
    CHECK(tx.Phase == LecDmaTxCancelled);
    CHECK(tx.MappingOwned == 0);
    return 0;
}

static int TestDescriptorsAndGate(void)
{
    LECS65_DMA_TRANSACTION tx;
    LECS65_DMA_LAYOUT_ENTRY table[512];
    LECS65_DMA_MAPPED_SEGMENT segment;
    LECS65_DMA_MAPPED_SEGMENT manySegments[512];
    unsigned i;
    uint64_t generation;
    unsigned localBarrier = 0;
    if (PrepareMapped(&tx, &generation)) return 1;
    segment.DeviceAddress = 0x100000000ULL;
    segment.LengthBytes = 4096;
    CHECK(LecDmaTransactionBuildDescriptors(&tx, generation, table,
        sizeof(table), 0x200000, &segment, 1) == LecDmaTxAddressRange);
    CHECK(tx.Phase == LecDmaTxMapped);
    segment.DeviceAddress = 0x100000;
    for (i = 0; i < 512; ++i) {
        manySegments[i].DeviceAddress = 0x100000 + (uint64_t)i * 0x1000;
        manySegments[i].LengthBytes = 8;
    }
    CHECK(LecDmaTransactionBuildDescriptors(&tx, generation, table,
        sizeof(table), 0x200000, manySegments, 512) == LecDmaTxCapacity);
    CHECK(LecDmaTransactionBuildDescriptors(&tx, generation, table,
        sizeof(table), 0x200000, &segment, 1) == LecDmaTxOk);
    CHECK(LecDmaTransactionPublish(&tx, generation, 0, 0) == LecDmaTxNotReady);
    CHECK(LecDmaTransactionPublish(&tx, generation, Barrier, &localBarrier) == LecDmaTxOk);
    CHECK(LecDmaTransactionBeginArm(&tx, generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionObserveCompletion(&tx, generation) == LecDmaTxNotReady);
    CHECK(LecDmaTransactionCommitHardwareLaunch(
        &tx, generation, 0, 0) == LecDmaTxLaunchDisabled);
    CHECK(tx.Phase == LecDmaTxPublished);
    CHECK(tx.EverPossiblyLaunched == 0);
    CHECK(tx.Diagnostics.LaunchRejected == 1);
    CHECK(LecDmaTransactionCancel(&tx, generation, 0) == LecDmaTxOk);
    CHECK(LecDmaTransactionMayRelease(&tx));
    CHECK(LecDmaTransactionReleaseMapping(&tx, generation) == LecDmaTxOk);
    CHECK(tx.Diagnostics.MappingAcquired == tx.Diagnostics.MappingReleased);
    return 0;
}

static int TestPendingCallbackCancellation(void)
{
    LECS65_DMA_TRANSACTION tx;
    uint64_t generation;
    LecDmaTransactionInitialize(&tx);
    CHECK(LecDmaTransactionSetLifecycle(&tx, 1, 1) == LecDmaTxOk);
    CHECK(LecDmaTransactionBegin(&tx, 1, 2, 4096, &generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionBindOwner(&tx, generation, 1, 2) == LecDmaTxOk);
    CHECK(LecDmaTransactionAdmitAdapter(&tx, generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionAcquireBuffer(&tx, generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionBeginMapping(&tx, generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionBeginPowerDown(&tx) == LecDmaTxOk);
    CHECK(tx.Phase == LecDmaTxCancelled);
    CHECK(tx.MappingCallbackPending == 1);
    CHECK(LecDmaTransactionMayRelease(&tx) == 0);
    CHECK(LecDmaTransactionMappingComplete(&tx, generation, 7, 1) == LecDmaTxOk);
    CHECK(tx.Phase == LecDmaTxCancelled && tx.MappingOwned);
    CHECK(LecDmaTransactionReleaseMapping(&tx, generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionBegin(&tx, 1, 2, 4096, &generation) == LecDmaTxNotReady);
    CHECK(LecDmaTransactionSetLifecycle(&tx, 1, 1) == LecDmaTxOk);
    CHECK(LecDmaTransactionBegin(&tx, 1, 2, 4096, &generation) == LecDmaTxOk);
    return 0;
}

static int TestPostGoQuarantine(void)
{
    LECS65_DMA_TRANSACTION tx;
    uint64_t generation;
    if (PreparePublished(&tx, &generation)) return 1;
    CHECK(LecDmaTransactionBeginArm(&tx, generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionCommitHardwareLaunch(
        &tx, generation, CommitGo, &generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionObserveCompletion(&tx, generation - 1) == LecDmaTxStaleGeneration);
    CHECK(tx.Phase == LecDmaTxDeviceActive);
    CHECK(LecDmaTransactionObserveCompletion(&tx, generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionObserveCompletion(&tx, generation) == LecDmaTxNotReady);
    CHECK(LecDmaTransactionMayRelease(&tx) == 0);
    CHECK(LecDmaTransactionProvePhysicalIdle(&tx, generation, 0) == LecDmaTxNotReady);
    CHECK(LecDmaTransactionCancel(&tx, generation, 1) == LecDmaTxMustQuarantine);
    CHECK(tx.Phase == LecDmaTxUnknownActive && tx.QuarantineLatched);
    CHECK(LecDmaTransactionReleaseMapping(&tx, generation) == LecDmaTxMustQuarantine);
    CHECK(LecDmaTransactionQuarantine(&tx, generation) == LecDmaTxOk);
    CHECK(tx.Phase == LecDmaTxQuarantined && tx.MappingOwned);
    return 0;
}

static int TestIdleProofAndRepeat(void)
{
    LECS65_DMA_TRANSACTION tx;
    LECS65_DMA_TRANSACTION snapshot;
    uint64_t generation;
    if (PreparePublished(&tx, &generation)) return 1;
    CHECK(LecDmaTransactionBeginArm(&tx, generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionCommitHardwareLaunch(
        &tx, generation, CommitGo, &generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionObserveCompletion(&tx, generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionProvePhysicalIdle(&tx, generation, 1) == LecDmaTxOk);
    CHECK(LecDmaTransactionMayRelease(&tx));
    CHECK(LecDmaTransactionReleaseMapping(&tx, generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionBegin(&tx, 0x11, 0x22, 4096, &generation) == LecDmaTxOk);
    CHECK(generation == 2);
    CHECK(LecDmaTransactionObserveCompletion(&tx, 1) == LecDmaTxStaleGeneration);
    LecDmaTransactionSnapshot(&tx, &snapshot);
    CHECK(snapshot.Generation == 2);
    CHECK(snapshot.Diagnostics.Requests == 2);
    return 0;
}

static int TestSurpriseAndPowerAfterGo(void)
{
    LECS65_DMA_TRANSACTION tx;
    uint64_t generation;
    if (PreparePublished(&tx, &generation)) return 1;
    CHECK(LecDmaTransactionBeginArm(&tx, generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionCommitHardwareLaunch(
        &tx, generation, CommitGo, &generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionBeginPnpTeardown(&tx, 1) == LecDmaTxMustQuarantine);
    CHECK(tx.Phase == LecDmaTxUnknownActive);
    CHECK(tx.Diagnostics.PnpTeardowns == 1);

    if (PreparePublished(&tx, &generation)) return 1;
    CHECK(LecDmaTransactionBeginArm(&tx, generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionCommitHardwareLaunch(
        &tx, generation, CommitGo, &generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionBeginPowerDown(&tx) == LecDmaTxMustQuarantine);
    CHECK(tx.Diagnostics.PowerTeardowns == 1);
    return 0;
}

static int TestCommitFailureIsTerminal(void)
{
    LECS65_DMA_TRANSACTION tx;
    uint64_t generation;
    if (PreparePublished(&tx, &generation)) return 1;
    CHECK(LecDmaTransactionBeginArm(&tx, generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionCommitHardwareLaunch(
        &tx, generation, FailCommit, 0) == LecDmaTxMustQuarantine);
    CHECK(tx.Phase == LecDmaTxUnknownActive && tx.EverPossiblyLaunched);
    CHECK(!LecDmaTransactionMayRelease(&tx));
    CHECK(LecDmaTransactionReleaseMapping(
        &tx, generation) == LecDmaTxMustQuarantine);
    return 0;
}

static int TestGoOrderingAndLateCompletion(void)
{
    LECS65_DMA_TRANSACTION tx;
    uint64_t generation;
    if (PrepareMapped(&tx, &generation)) return 1;
    CHECK(LecDmaTransactionCommitHardwareLaunch(
        &tx, generation, CommitGo, &generation) == LecDmaTxNotReady);

    if (PreparePublished(&tx, &generation)) return 1;
    CHECK(LecDmaTransactionBeginArm(&tx, generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionCommitHardwareLaunch(
        &tx, generation, CommitGo, &generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionCancel(&tx, generation, 1) == LecDmaTxMustQuarantine);
    CHECK(LecDmaTransactionObserveCompletion(
        &tx, generation) == LecDmaTxNotReady);
    CHECK(!LecDmaTransactionMayRelease(&tx));
    return 0;
}

int main(void)
{
    if (TestAdmissionAndRollback()) return 1;
    if (TestDescriptorsAndGate()) return 1;
    if (TestPendingCallbackCancellation()) return 1;
    if (TestPostGoQuarantine()) return 1;
    if (TestIdleProofAndRepeat()) return 1;
    if (TestSurpriseAndPowerAfterGo()) return 1;
    if (TestCommitFailureIsTerminal()) return 1;
    if (TestGoOrderingAndLateCompletion()) return 1;
    CHECK(barrier_calls >= 4);
    printf("DMA transaction tests: %u/%u checks passed; hardware launch unavailable\n",
        checks, checks);
    return 0;
}
