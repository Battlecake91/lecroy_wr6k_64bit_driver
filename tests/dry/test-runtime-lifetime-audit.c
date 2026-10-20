#include <stdio.h>
#include <string.h>

typedef struct EVENT_MODEL {
    unsigned References;
    unsigned Signals;
} EVENT_MODEL;

typedef struct EVENT_SLOT_MODEL {
    EVENT_MODEL* Event;
} EVENT_SLOT_MODEL;

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

int main(void)
{
    EVENT_MODEL a = { 1, 0 };
    EVENT_MODEL b = { 1, 0 };
    EVENT_SLOT_MODEL slot = { 0 };
    int statuses[5] = { 0, 0, -1, 0, 0 };
    int enabled[5] = { 0, 0, 0, 0, 0 };
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

    failures = enable_interfaces(statuses, enabled);
    check("interface fault fixture injects exactly one failure",
          failures == 1);
    check("current continue-on-error policy leaves partial publication",
          enabled[0] && enabled[1] && !enabled[2] && enabled[3] && enabled[4]);

    printf("RUNTIME LIFETIME AUDIT: %u/%u passed; %u failed.\n",
           passed, passed + failed, failed);
    return failed ? 1 : 0;
}
