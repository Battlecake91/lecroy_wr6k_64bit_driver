
void __fastcall FUN_00019ba6(int param_1)

{
  undefined1 uVar1;
  
  uVar1 = KfAcquireSpinLock();
  *(undefined1 *)(param_1 + 0x10) = uVar1;
  return;
}

