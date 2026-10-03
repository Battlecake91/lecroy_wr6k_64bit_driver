
void __fastcall FUN_00018b9e(int param_1)

{
  if ((DAT_0001d258 != (code *)0x0) && (*(int *)(param_1 + 0x14) != 0)) {
    (*DAT_0001d258)(*(int *)(param_1 + 0x14));
  }
  if ((*(char *)(param_1 + 0x24) != '\0') && (*(int *)(param_1 + 0xc) != 0)) {
    ExFreePool(*(int *)(param_1 + 0xc));
  }
  return;
}

