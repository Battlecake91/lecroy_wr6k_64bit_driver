
undefined4 * __fastcall FUN_00011a00(undefined4 *param_1)

{
  undefined1 *puVar1;
  int iVar2;
  undefined4 *puVar3;
  
  FUN_00011962(param_1);
  *(undefined1 *)(param_1 + 8) = 2;
  puVar3 = param_1 + 10;
  for (iVar2 = 0x100; iVar2 != 0; iVar2 = iVar2 + -1) {
    *puVar3 = 0xffffffff;
    puVar3 = puVar3 + 1;
  }
  iVar2 = 0;
  puVar1 = (undefined1 *)((int)param_1 + 0x2a);
  do {
    *puVar1 = (char)iVar2;
    iVar2 = iVar2 + 1;
    puVar1 = puVar1 + 4;
  } while (iVar2 < 0x100);
  return param_1;
}

