
uint __fastcall FUN_00015364(int param_1)

{
  uint uVar1;
  
  uVar1 = *(ushort *)(*(int *)(param_1 + 0x10) + 2) + 8 + *(int *)(param_1 + 0x10);
  *(uint *)(param_1 + 0x10) = uVar1;
  return -(uint)(uVar1 < (uint)(*(int *)(param_1 + 0xc) + *(int *)(param_1 + 8))) & uVar1;
}

