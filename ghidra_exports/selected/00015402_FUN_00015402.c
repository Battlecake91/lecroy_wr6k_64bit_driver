
void __fastcall FUN_00015402(int param_1)

{
  if (*(int *)(param_1 + 8) != 0) {
    ExFreePool(*(int *)(param_1 + 8));
    *(undefined4 *)(param_1 + 8) = 0;
    *(undefined4 *)(param_1 + 0x10) = 0;
    *(undefined4 *)(param_1 + 0xc) = 0;
  }
  return;
}

