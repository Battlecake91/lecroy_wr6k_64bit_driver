
undefined1 __fastcall FUN_00019a00(int param_1)

{
  undefined1 uVar1;
  
  uVar1 = 0;
  if (((*(byte *)(param_1 + 0x10c) & 1) != 0) && (*(int *)(param_1 + 0x1ac) != 0)) {
    uVar1 = IoCancelIrp(*(int *)(param_1 + 0x1ac));
  }
  return uVar1;
}

