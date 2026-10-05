#include "DmaMappingOwner.h"

void LecMapOwnerInit(LECS65_MAPPING_OWNER* o)
{
    if (!o) return;
    o->Phase = LecMapEmpty;
    o->CallbackSeen = 0;
    o->EverLaunched = 0;
}

int LecMapOwnerRequest(LECS65_MAPPING_OWNER* o)
{
    if (!o || o->Phase != LecMapEmpty) return 0;
    o->Phase = LecMapAwaitingCallback;
    return 1;
}

int LecMapOwnerCallback(LECS65_MAPPING_OWNER* o)
{
    if (!o) return 0;
    if (o->Phase != LecMapAwaitingCallback &&
        o->Phase != LecMapUnknownActive) return 0;
    o->CallbackSeen = 1;
    if (o->Phase == LecMapAwaitingCallback)
        o->Phase = LecMapReady;
    return 1;
}

/* Only use if the DMA DDI guarantees no future callback after failure. */
int LecMapOwnerRequestFailed(LECS65_MAPPING_OWNER* o)
{
    if (!o || o->Phase != LecMapAwaitingCallback ||
        o->CallbackSeen) return 0;
    o->Phase = LecMapEmpty;
    return 1;
}

int LecMapOwnerLaunch(LECS65_MAPPING_OWNER* o)
{
    if (!o || o->Phase != LecMapReady) return 0;
    o->EverLaunched = 1;
    o->Phase = LecMapDeviceActive;
    return 1;
}

/*
 * This can only be called with a separately established *bus idle*
 * guarantee. Receipt of an IRQ, timeout or DPC callback is not proof.
 */
int LecMapOwnerIdleProved(LECS65_MAPPING_OWNER* o)
{
    if (!o || o->Phase != LecMapDeviceActive) return 0;
    o->Phase = LecMapReady;
    return 1;
}

/* Fail-closed terminal state; never unlock or return a mapping. */
void LecMapOwnerUncertain(LECS65_MAPPING_OWNER* o)
{
    if (!o || o->Phase == LecMapReturned) return;
    o->Phase = LecMapUnknownActive;
}

int LecMapOwnerMayRelease(const LECS65_MAPPING_OWNER* o)
{
    return o && o->Phase == LecMapReady && o->CallbackSeen;
}

int LecMapOwnerReleased(LECS65_MAPPING_OWNER* o)
{
    if (!LecMapOwnerMayRelease(o)) return 0;
    o->Phase = LecMapReturned;
    return 1;
}
