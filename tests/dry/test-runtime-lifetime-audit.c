#include <stdio.h>
#include <string.h>

typedef struct EVENT_MODEL {
    unsigned References;
    unsigned Signals;
} EVENT_MODEL;

typedef struct EVENT_SLOT_MODEL {
    EVENT_MODEL* Event;
} EVENT_SLOT_MODEL;

typedef enum DMA_STATE_MODEL {
    DmaInactive,
    DmaUnknownActive
} DMA_STATE_MODEL;

typedef struct DMA_PROCESS_MODEL {
    unsigned OpenReferences;
    unsigned InactiveRegistrations;
    unsigned UnknownActiveRegistrations;
} DMA_PROCESS_MODEL;

static unsigned passed;
static unsigned failed;

static void check(const char* name, int condition)
{
    printf("[%s] %s\n", condition ? "PASS" : "FAIL", name);
    if (condition) {
        ++passed;
    }
    else {
        ++failed;
    }
}

static void reference_event(EVENT_MODEL* event)
{
    ++event->References;
}

static void dereference_event(EVENT_MODEL* event)
{
    if (event != NULL) {
        --event->References;
    }
}

/* Models the reference-before-publish and dereference-after-swap ordering. */
static void replace_event(EVENT_SLOT_MODEL* slot, EVENT_MODEL* replacement)
{
    EVENT_MODEL* previous;

    reference_event(replacement);
    previous = slot->Event;
    slot->Event = replacement;
    dereference_event(previous);
}

/* Models DPC signaling while the production LegacyEventLock is held. */
static void signal_current(EVENT_SLOT_MODEL* slot)
{
    if (slot->Event != NULL) {
        ++slot->Event->Signals;
    }
}

static void detach_event(EVENT_SLOT_MODEL* slot)
{
    EVENT_MODEL* previous = slot->Event;
    slot->Event = NULL;
    dereference_event(previous);
}

static unsigned enable_interfaces(
    const int statuses[5],
    int enabled[5])
{
    unsigned i;
    unsigned failures = 0;

    for (i = 0; i < 5; ++i) {
        if (statuses[i] == 0) {
            enabled[i] = 1;
        }
        else {
            ++failures;
        }
    }

    return failures;
}

static void disable_interfaces(int enabled[5])
{
    unsigned i;

    for (i = 0; i < 5; ++i) {
        if (enabled[i]) {
            enabled[i] = 0;
        }
    }
}

static int all_interfaces_disabled(const int enabled[5])
{
    unsigned i;

    for (i = 0; i < 5; ++i) {
        if (enabled[i]) {
            return 0;
        }
    }
    return 1;
}

/*
 * Characterizes the proposed ABI-compatible ownership rule.  This is not a
 * claim that the current driver implements per-process open references.
 */
static void dma_create(DMA_PROCESS_MODEL* process)
{
    ++process->OpenReferences;
}

static void dma_register(DMA_PROCESS_MODEL* process, DMA_STATE_MODEL state)
{
    if (state == DmaInactive) {
        ++process->InactiveRegistrations;
    }
    else {
        ++process->UnknownActiveRegistrations;
    }
}

static void dma_cleanup(DMA_PROCESS_MODEL* process)
{
    (void)process; /* CLEANUP is file-scoped and must not retire process state. */
}

static void dma_close(DMA_PROCESS_MODEL* process)
{
    if (process->OpenReferences == 0) {
        return;
    }

    --process->OpenReferences;
    if (process->OpenReferences == 0) {
        process->InactiveRegistrations = 0;
    }
}

static void dma_pnp_teardown(DMA_PROCESS_MODEL* process)
{
    process->InactiveRegistrations = 0;
    /* UnknownActive remains quarantined while physical bus-idle is unproven. */
}

int main(void)
{
    EVENT_MODEL a = { 1, 0 };
    EVENT_MODEL b = { 1, 0 };
    EVENT_SLOT_MODEL slot = { 0 };
    int statuses[5];
    int enabled[5] = { 0, 0, 0, 0, 0 };
    DMA_PROCESS_MODEL dma = { 0, 0, 0 };
    unsigned failures;

    replace_event(&slot, &a);
    dereference_event(&a); /* User-mode handle closes. */
    signal_current(&slot);
    check("driver reference survives registering handle close",
          a.References == 1 && a.Signals == 1);

    signal_current(&slot); /* DPC wins the lock before replacement. */
    replace_event(&slot, &b);
    check("DPC-before-replacement signals only the old slot",
          a.Signals == 2 && b.Signals == 0 && a.References == 0);

    signal_current(&slot); /* Replacement wins the lock before the DPC. */
    check("replacement-before-DPC signals only the new slot",
          a.Signals == 2 && b.Signals == 1 && b.References == 2);

    dereference_event(&b); /* User-mode handle closes. */
    detach_event(&slot);
    signal_current(&slot);
    check("teardown detaches before final dereference",
          slot.Event == NULL && b.References == 0 && b.Signals == 1);

    /* Two user handles may close independently of the driver's references. */
    a.References = 2;
    a.Signals = 0;
    replace_event(&slot, &a);
    dereference_event(&a);
    signal_current(&slot);
    check("closing one of several user handles preserves the published event",
          a.References == 2 && a.Signals == 1 && slot.Event == &a);
    dereference_event(&a);

    b.References = 1; /* New user handle. */
    replace_event(&slot, &b); /* Replacement wins the serialized lock. */
    signal_current(&slot);
    check("concurrent replacement has one current referenced event",
          slot.Event == &b && a.References == 0 && b.Signals == 2);

    dereference_event(&b);
    signal_current(&slot); /* Models a DPC serialized during user-handle close. */
    check("DPC during handle close uses the retained driver reference",
          b.References == 1 && b.Signals == 3);

    replace_event(&slot, &a); /* In-flight registration completes before drain. */
    detach_event(&slot);      /* STOP/REMOVE closes admission, drains, detaches. */
    check("STOP or REMOVE after event replacement leaves no published slot",
          slot.Event == NULL && a.References == 0 && b.References == 0);

    statuses[0] = -1; statuses[1] = 0; statuses[2] = 0;
    statuses[3] = 0; statuses[4] = 0;
    memset(enabled, 0, sizeof(enabled));
    failures = enable_interfaces(statuses, enabled);
    check("first interface enable failure is isolated",
          failures == 1 && !enabled[0] && enabled[1] && enabled[4]);
    disable_interfaces(enabled);
    check("STOP or REMOVE disables first-failure partial publication",
          all_interfaces_disabled(enabled));

    statuses[0] = 0; statuses[1] = 0; statuses[2] = -1;
    statuses[3] = 0; statuses[4] = 0;
    memset(enabled, 0, sizeof(enabled));
    failures = enable_interfaces(statuses, enabled);
    check("middle interface failure preserves current continue-on-error policy",
          enabled[0] && enabled[1] && !enabled[2] && enabled[3] && enabled[4]);

    statuses[0] = 0; statuses[1] = 0; statuses[2] = 0;
    statuses[3] = 0; statuses[4] = -1;
    memset(enabled, 0, sizeof(enabled));
    failures = enable_interfaces(statuses, enabled);
    check("last interface enable failure is isolated",
          failures == 1 && enabled[0] && enabled[3] && !enabled[4]);

    statuses[0] = -1; statuses[1] = 0; statuses[2] = -1;
    statuses[3] = -1; statuses[4] = 0;
    memset(enabled, 0, sizeof(enabled));
    failures = enable_interfaces(statuses, enabled);
    check("multiple interface failures preserve only successful publications",
          failures == 3 && !enabled[0] && enabled[1] && !enabled[2] &&
          !enabled[3] && enabled[4]);
    disable_interfaces(enabled);
    disable_interfaces(enabled);
    check("partial publication teardown is idempotent",
          all_interfaces_disabled(enabled));

    dma_create(&dma);
    dma_create(&dma);
    dma_register(&dma, DmaInactive);
    dma_close(&dma);
    check("first same-process CLOSE preserves inactive DMA registration",
          dma.OpenReferences == 1 && dma.InactiveRegistrations == 1);
    dma_cleanup(&dma);
    check("file CLEANUP does not retire process-owned DMA registration",
          dma.OpenReferences == 1 && dma.InactiveRegistrations == 1);
    dma_close(&dma);
    check("final same-process CLOSE retires inactive DMA registration",
          dma.OpenReferences == 0 && dma.InactiveRegistrations == 0);

    dma_create(&dma);
    dma_register(&dma, DmaInactive);
    dma_register(&dma, DmaUnknownActive);
    dma_pnp_teardown(&dma);
    check("PnP teardown releases inactive but retains UnknownActive quarantine",
          dma.InactiveRegistrations == 0 && dma.UnknownActiveRegistrations == 1);
    dma_close(&dma);
    check("final CLOSE cannot retire UnknownActive quarantine",
          dma.OpenReferences == 0 && dma.UnknownActiveRegistrations == 1);

    printf("RUNTIME LIFETIME AUDIT: %u/%u passed; %u failed.\n",
           passed, passed + failed, failed);
    return failed ? 1 : 0;
}
