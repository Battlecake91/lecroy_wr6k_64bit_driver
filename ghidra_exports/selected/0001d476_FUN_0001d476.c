
void __fastcall FUN_0001d476(undefined4 *param_1)

{
  ushort *puVar1;
  int iVar2;
  
  *param_1 = &PTR_LAB_0001ca00;
  if (param_1[1] != 0) {
    puVar1 = (ushort *)(param_1 + 6);
    if ((*puVar1 & 0xfffe) != 0) {
      IoDeleteSymbolicLink(puVar1);
      FUN_000105cc(puVar1);
    }
    iVar2 = param_1[1];
    if (*(int *)(iVar2 + 0x28) != iVar2 + 0xb8) {
      *(undefined4 *)(iVar2 + 0x28) = 0;
    }
  }
  if (param_1[7] != 0) {
    FUN_000105cc((undefined2 *)(param_1 + 6));
  }
  if (param_1[4] != 0) {
    FUN_000105cc((undefined2 *)(param_1 + 3));
  }
  FUN_000183b8(param_1);
  return;
}

