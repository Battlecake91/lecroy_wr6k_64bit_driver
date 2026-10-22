#pragma once
/*
 * Pure software ownership model for a future asynchronous WDM SG mapping.
 * No DDI, MMIO, PCI access, DMA control, or resource release here.
 * Callers must serialize transitions under a suitable lock.
 */
typedef enum _LECS65_MAPPING_PHASE {
    LecMapEmpty = 0,
    LecMapAwaitingCallback,
    LecMapReady,
    LecMapDeviceActive,
    LecMapUnknownActive,
    LecMapReturned
} LECS65_MAPPING_PHASE;

typedef struct _LECS65_MAPPING_OWNER {
    LECS65_MAPPING_PHASE Phase;
    unsigned CallbackSeen;
    unsigned EverLaunched;
} LECS65_MAPPING_OWNER;

void LecMapOwnerInit(LECS65_MAPPING_OWNER* Owner);
int LecMapOwnerRequest(LECS65_MAPPING_OWNER* Owner);
int LecMapOwnerCallback(LECS65_MAPPING_OWNER* Owner);
int LecMapOwnerRequestFailed(LECS65_MAPPING_OWNER* Owner);
int LecMapOwnerLaunch(LECS65_MAPPING_OWNER* Owner);
int LecMapOwnerIdleProved(LECS65_MAPPING_OWNER* Owner);
void LecMapOwnerUncertain(LECS65_MAPPING_OWNER* Owner);
int LecMapOwnerMayRelease(const LECS65_MAPPING_OWNER* Owner);
int LecMapOwnerReleased(LECS65_MAPPING_OWNER* Owner);
