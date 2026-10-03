
void __fastcall FUN_000106a0(int param_1)

{
  if (((*(char *)(param_1 + 0x18) != '\0') &&
      (*(undefined1 *)(param_1 + 0x18) = 0, *(int *)(param_1 + 4) == 0)) &&
     (*(int *)(param_1 + 0x10) != -1)) {
    MmUnmapIoSpace(*(int *)(param_1 + 0x10),*(undefined4 *)(param_1 + 0x14));
    *(undefined4 *)(param_1 + 0x10) = 0;
  }
  return;
}

