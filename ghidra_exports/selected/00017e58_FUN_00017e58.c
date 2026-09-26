
void __fastcall FUN_00017e58(int param_1)

{
  if ((*(int *)(param_1 + 0x6c) != 0) || (*(int *)(param_1 + 0x60) != 0)) {
    FUN_00012166((undefined4 *)(param_1 + 0x5c));
  }
  if ((*(int *)(param_1 + 0x50) != 0) || (*(int *)(param_1 + 0x44) != 0)) {
    FUN_00012166((undefined4 *)(param_1 + 0x40));
  }
  if ((*(int *)(param_1 + 0x34) != 0) || (*(int *)(param_1 + 0x28) != 0)) {
    FUN_00012166((undefined4 *)(param_1 + 0x24));
  }
  if ((*(int *)(param_1 + 0x18) == 0) && (*(int *)(param_1 + 0xc) == 0)) {
    return;
  }
  FUN_00012166((undefined4 *)(param_1 + 8));
  return;
}

