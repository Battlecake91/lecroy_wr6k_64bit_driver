#include <stdio.h>
#include <string.h>

#define MAX_PROCESSES 4
#define MAX_FILES 8

typedef enum DMA_REGISTRATION_STATE {
    DmaRegistrationInactive,
    DmaRegistrationUnknownActive
} DMA_REGISTRATION_STATE;

typedef struct PROCESS_MODEL {
    unsigned Pid;
    unsigned OpenReferences;
    unsigned InactiveRegistrations;
    unsigned UnknownActiveRegistrations;
} PROCESS_MODEL;

typedef struct FILE_MODEL {
    PROCESS_MODEL* Process;
    int Associated;
    int Closed;
} FILE_MODEL;

typedef struct DEVICE_MODEL {
    PROCESS_MODEL Processes[MAX_PROCESSES];
    FILE_MODEL Files[MAX_FILES];
    unsigned ProcessCount;
    unsigned FileCount;
    int AdmissionOpen;
} DEVICE_MODEL;

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

static void initialize_device(DEVICE_MODEL* device)
{
    memset(device, 0, sizeof(*device));
    device->AdmissionOpen = 1;
}

static PROCESS_MODEL* find_process(DEVICE_MODEL* device, unsigned pid)
{
    unsigned i;
    for (i = 0; i < device->ProcessCount; ++i) {
        if (device->Processes[i].Pid == pid) {
            return &device->Processes[i];
        }
    }
    return NULL;
}

/*
 * Models the proposed serialized CREATE commit. Allocation and association
 * must succeed before the process open-reference becomes visible.
 */
static FILE_MODEL* create_file(DEVICE_MODEL* device, unsigned pid,
                               int processAllocationSucceeds,
                               int fileAssociationSucceeds)
{
    PROCESS_MODEL* process;
    FILE_MODEL* file;

    if (!device->AdmissionOpen || !processAllocationSucceeds ||
        !fileAssociationSucceeds || device->FileCount == MAX_FILES) {
        return NULL;
    }

    process = find_process(device, pid);
    if (process == NULL) {
        if (device->ProcessCount == MAX_PROCESSES) {
            return NULL;
        }
        process = &device->Processes[device->ProcessCount++];
        memset(process, 0, sizeof(*process));
        process->Pid = pid;
    }

    file = &device->Files[device->FileCount++];
    memset(file, 0, sizeof(*file));
    file->Process = process;
    file->Associated = 1;
    ++process->OpenReferences;
    return file;
}

static int register_dma(FILE_MODEL* file, DMA_REGISTRATION_STATE state)
{
    if (file == NULL || !file->Associated || file->Closed ||
        file->Process == NULL || file->Process->OpenReferences == 0) {
        return 0;
    }

    if (state == DmaRegistrationInactive) {
        ++file->Process->InactiveRegistrations;
    }
    else {
        ++file->Process->UnknownActiveRegistrations;
    }
    return 1;
}

static void cleanup_file(FILE_MODEL* file)
{
    (void)file; /* CLEANUP cannot retire process-owned registrations. */
}

static void close_file(FILE_MODEL* file)
{
    PROCESS_MODEL* process;

    if (file == NULL || file->Closed || !file->Associated) {
        return;
    }

    process = file->Process; /* Association, never caller context, owns CLOSE. */
    file->Closed = 1;
    file->Associated = 0;
    if (process == NULL || process->OpenReferences == 0) {
        return;
    }

    --process->OpenReferences;
    if (process->OpenReferences == 0) {
        process->InactiveRegistrations = 0;
        /* No physical bus-idle proof: UnknownActive is never retired here. */
    }
}

static void close_file_from_process(FILE_MODEL* file, unsigned callerPid)
{
    (void)callerPid;
    close_file(file); /* Recorded file association, not caller context, owns it. */
}

static void surprise_remove(DEVICE_MODEL* device)
{
    unsigned i;
    device->AdmissionOpen = 0;
    for (i = 0; i < device->ProcessCount; ++i) {
        device->Processes[i].InactiveRegistrations = 0;
        /* UnknownActive remains quarantined without physical idle proof. */
    }
}

int main(void)
{
    DEVICE_MODEL device;
    FILE_MODEL* a;
    FILE_MODEL* b;
    FILE_MODEL* c;
    PROCESS_MODEL* process;

    initialize_device(&device);
    a = create_file(&device, 100, 0, 1);
    check("CREATE process-allocation failure rolls back without association",
          a == NULL && device.ProcessCount == 0 && device.FileCount == 0);
    a = create_file(&device, 100, 1, 0);
    check("CREATE file-association failure rolls back without open reference",
          a == NULL && device.ProcessCount == 0 && device.FileCount == 0);

    a = create_file(&device, 100, 1, 1);
    process = find_process(&device, 100);
    check("successful CREATE associates file with one process reference",
          a != NULL && a->Process == process && a->Associated &&
          process->OpenReferences == 1);
    b = create_file(&device, 100, 1, 1);
    check("two handles from one process share process ownership",
          b != NULL && b->Process == process && process->OpenReferences == 2);
    check("registration is attributed through the file association",
          register_dma(a, DmaRegistrationInactive) &&
          process->InactiveRegistrations == 1);
    cleanup_file(a);
    check("CLEANUP does not retire process-owned DMA registration",
          process->OpenReferences == 2 && process->InactiveRegistrations == 1);
    close_file(a);
    check("first same-process CLOSE preserves inactive registration",
          process->OpenReferences == 1 && process->InactiveRegistrations == 1);
    close_file(a);
    check("duplicate CLOSE is idempotent",
          process->OpenReferences == 1 && process->InactiveRegistrations == 1);
    close_file(b);
    check("last same-process CLOSE retires only inactive registration",
          process->OpenReferences == 0 && process->InactiveRegistrations == 0);
    check("registration through a closed file is rejected",
          !register_dma(b, DmaRegistrationInactive));

    initialize_device(&device);
    a = create_file(&device, 200, 1, 1);
    b = create_file(&device, 200, 1, 1);
    process = find_process(&device, 200);
    check("register-before-first-CLOSE ordering preserves registration",
          register_dma(a, DmaRegistrationInactive));
    close_file(b);
    check("register-before-first-CLOSE leaves one owning handle",
          process->OpenReferences == 1 && process->InactiveRegistrations == 1);
    check("first-CLOSE-before-register ordering accepts remaining handle",
          register_dma(a, DmaRegistrationInactive) &&
          process->InactiveRegistrations == 2);
    close_file(a);
    check("serialized register-before-final-CLOSE retires inactive state",
          process->OpenReferences == 0 && process->InactiveRegistrations == 0);
    check("serialized final-CLOSE-before-register rejects stale file",
          !register_dma(a, DmaRegistrationInactive));

    initialize_device(&device);
    a = create_file(&device, 300, 1, 1);
    b = create_file(&device, 301, 1, 1);
    register_dma(a, DmaRegistrationInactive);
    register_dma(b, DmaRegistrationInactive);
    close_file(a);
    check("last CLOSE is process-local and does not retire another process",
          find_process(&device, 300)->InactiveRegistrations == 0 &&
          find_process(&device, 301)->InactiveRegistrations == 1 &&
          find_process(&device, 301)->OpenReferences == 1);

    initialize_device(&device);
    a = create_file(&device, 350, 1, 1);
    process = find_process(&device, 350);
    register_dma(a, DmaRegistrationInactive);
    close_file_from_process(a, 999);
    check("CLOSE caller context cannot change recorded ownership",
          process->OpenReferences == 0 &&
          process->InactiveRegistrations == 0);

    initialize_device(&device);
    a = create_file(&device, 400, 1, 1);
    process = find_process(&device, 400);
    register_dma(a, DmaRegistrationUnknownActive);
    close_file(a);
    check("final CLOSE retains UnknownActive without physical idle proof",
          process->OpenReferences == 0 &&
          process->UnknownActiveRegistrations == 1);

    initialize_device(&device);
    a = create_file(&device, 500, 1, 1);
    b = create_file(&device, 500, 1, 1);
    process = find_process(&device, 500);
    register_dma(a, DmaRegistrationInactive);
    register_dma(b, DmaRegistrationUnknownActive);
    surprise_remove(&device);
    check("surprise removal closes CREATE admission",
          !device.AdmissionOpen && create_file(&device, 500, 1, 1) == NULL);
    check("surprise removal retires inactive but retains UnknownActive",
          process->InactiveRegistrations == 0 &&
          process->UnknownActiveRegistrations == 1);
    close_file(a);
    close_file(b);
    check("post-surprise CLOSE drains references but not quarantine",
          process->OpenReferences == 0 &&
          process->UnknownActiveRegistrations == 1);

    initialize_device(&device);
    a = create_file(&device, 600, 1, 1);
    surprise_remove(&device);
    close_file(a);
    check("CREATE committed before removal is later drained exactly once",
          find_process(&device, 600)->OpenReferences == 0);
    c = create_file(&device, 601, 1, 1);
    check("CREATE serialized after removal is rejected without association",
          c == NULL && find_process(&device, 601) == NULL);

    printf("PROCESS DMA LIFETIME MODEL: %u/%u passed; %u failed.\n",
           passed, passed + failed, failed);
    return failed ? 1 : 0;
}
