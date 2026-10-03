
void __thiscall FUN_0001a196(void *this,int param_1)

{
  *(undefined4 *)(param_1 + 0x1c) = 0;
  *(undefined4 *)(param_1 + 0x18) = 0xc0000120;
  FUN_00019ffe((void *)((int)this + 0xac),param_1);
  IoReleaseCancelSpinLock(*(undefined1 *)(param_1 + 0x25));
  *(undefined4 *)(param_1 + 0x18) = 0xc0000120;
  IofCompleteRequest();
  return;
}

