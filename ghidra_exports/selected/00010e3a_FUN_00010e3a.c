
void __fastcall FUN_00010e3a(int param_1)

{
  int iVar1;
  
  iVar1 = IoGetCurrentProcess();
  FUN_00010a8e((void *)(param_1 + 0x1078),iVar1);
  iVar1 = FUN_00010798(&stack0x00000004,param_1,0);
  if (iVar1 == 0) {
    DAT_0001ce0c = DAT_0001ce0c + 1;
  }
  return;
}

