
undefined4 __fastcall FUN_0001829a(int param_1)

{
  int iVar1;
  
  iVar1 = *(int *)(param_1 + 4);
  while (iVar1 != 0) {
    iVar1 = *(int *)(iVar1 + 0x3c);
    FUN_00017fd6();
  }
  *(undefined4 *)(param_1 + 4) = 0;
  return 0;
}

