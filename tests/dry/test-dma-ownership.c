#include <stdio.h>
#include "../../driver/DmaMappingOwner.h"

static unsigned ok;
static unsigned bad;
static void check(const char* name, int success)
{
    printf("[%s] %s\n", success ? "PASS" : "FAIL", name);
    if (success) ++ok; else ++bad;
}

int main(void)
{
    LECS65_MAPPING_OWNER o;

    LecMapOwnerInit(&o);
    check("fresh mapping cannot be returned",
          !LecMapOwnerMayRelease(&o));
    check("request sets pending state",
          LecMapOwnerRequest(&o) &&
          o.Phase == LecMapAwaitingCallback);
    check("pending callback prevents release",
          !LecMapOwnerMayRelease(&o) && !LecMapOwnerReleased(&o));
    check("duplicate mapping request denied",
          !LecMapOwnerRequest(&o));
    check("callback completes staged mapping",
          LecMapOwnerCallback(&o) && LecMapOwnerMayRelease(&o));
    check("no duplicate callback",
          !LecMapOwnerCallback(&o));
    check("device start owns mapping",
          LecMapOwnerLaunch(&o) && !LecMapOwnerMayRelease(&o));
    check("cannot free active device mapping",
          !LecMapOwnerReleased(&o));
    check("proven idle permits mapping release",
          LecMapOwnerIdleProved(&o) && LecMapOwnerReleased(&o));
    check("cannot reuse released mapping",
          !LecMapOwnerRequest(&o) && !LecMapOwnerReleased(&o));

    LecMapOwnerInit(&o);
    (void)LecMapOwnerRequest(&o);
    check("request failure may cancel before callback",
          LecMapOwnerRequestFailed(&o) && o.Phase == LecMapEmpty);
    check("remapping after failed request",
          LecMapOwnerRequest(&o) && LecMapOwnerCallback(&o));
    (void)LecMapOwnerLaunch(&o);
    LecMapOwnerUncertain(&o);
    check("timeout or unknown DMA permanently quarantines",
          o.Phase == LecMapUnknownActive &&
          !LecMapOwnerMayRelease(&o) &&
          !LecMapOwnerIdleProved(&o));
    check("repeated uncertainty remains terminal",
          (LecMapOwnerUncertain(&o), o.Phase == LecMapUnknownActive));
    check("cannot reset terminal ownership",
          !LecMapOwnerRequest(&o) && !LecMapOwnerReleased(&o));

    LecMapOwnerInit(&o);
    (void)LecMapOwnerRequest(&o);
    LecMapOwnerUncertain(&o);
    check("REMOVE during pending callback cannot release",
          !LecMapOwnerMayRelease(&o));
    check("late callback cannot clear quarantine",
          LecMapOwnerCallback(&o) && o.CallbackSeen &&
          o.Phase == LecMapUnknownActive &&
          !LecMapOwnerMayRelease(&o));

    printf("DMA OWNERSHIP: %u/%u passed; %u failed.\n",
           ok, ok + bad, bad);
    return bad ? 1 : 0;
}
