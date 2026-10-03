
void __fastcall FUN_000103c8(int param_1)

{
  int iVar1;
  
  iVar1 = *(int *)(param_1 + 4);
  *(undefined4 *)(param_1 + 4) = 0;
  if (iVar1 != 0) {
    ZwClose(iVar1);
  }
  return;
}

