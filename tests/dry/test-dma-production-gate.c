#include "../../driver/DmaTransaction.h"
#include <stdio.h>

#if LECS65_ENABLE_WDM_V3_MAM_DMA
#error The production gate test must compile with the default closed gate.
#endif

static unsigned checks;
static unsigned commit_calls;

#define CHECK(x) do { checks++; if (!(x)) { \
    printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

static void Barrier(void* context)
{
    unsigned* calls = (unsigned*)context;
    (*calls)++;
}

static unsigned UnexpectedCommit(void* context, uint64_t generation)
{
    (void)context;
    (void)generation;
    commit_calls++;
    return 1;
}

int main(void)
{
    LECS65_DMA_TRANSACTION tx;
    LECS65_DMA_LAYOUT_ENTRY table[512];
    LECS65_DMA_MAPPED_SEGMENT segment = { 0x00100000, 4096 };
    uint64_t generation = 0;
    unsigned barrierCalls = 0;

    LecDmaTransactionInitialize(&tx);
    CHECK(LecDmaTransactionSetLifecycle(&tx, 1, 1) == LecDmaTxOk);
    CHECK(LecDmaTransactionBegin(&tx, 1, 2, 4096, &generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionBindOwner(&tx, generation, 1, 2) == LecDmaTxOk);
    CHECK(LecDmaTransactionAdmitAdapter(&tx, generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionAcquireBuffer(&tx, generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionBeginMapping(&tx, generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionMappingComplete(
        &tx, generation, 3, 1) == LecDmaTxOk);
    CHECK(LecDmaTransactionBuildDescriptors(
        &tx, generation, table, sizeof(table), 0x00200000,
        &segment, 1) == LecDmaTxOk);
    CHECK(LecDmaTransactionPublish(
        &tx, generation, Barrier, &barrierCalls) == LecDmaTxOk);
    CHECK(barrierCalls == 1);
    CHECK(LecDmaTransactionBeginArm(&tx, generation) == LecDmaTxOk);
    CHECK(LecDmaTransactionCommitHardwareLaunch(
        &tx, generation, UnexpectedCommit, 0) == LecDmaTxLaunchDisabled);
    CHECK(commit_calls == 0);
    CHECK(tx.EverPossiblyLaunched == 0 && tx.Phase == LecDmaTxPublished);
    CHECK(LecDmaTransactionCancel(&tx, generation, 0) == LecDmaTxOk);
    CHECK(LecDmaTransactionReleaseMapping(&tx, generation) == LecDmaTxOk);

    printf("DMA production launch gate: %u/%u checks passed; callback unreachable\n",
        checks, checks);
    return 0;
}
