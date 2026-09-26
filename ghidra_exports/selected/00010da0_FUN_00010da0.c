
void __thiscall FUN_00010da0(void *this,int param_1)

{
  int iVar1;
  int iVar2;
  char cVar3;
  int local_c;
  uint local_8;
  
  iVar2 = param_1;
  iVar1 = *(int *)((int)this + 4);
  local_c = iVar1 + 0x60;
  local_8 = (uint)this & 0xffff0000;
  if (param_1 == *(int *)(iVar1 + 0x14)) {
    *(undefined4 *)(iVar1 + 0x14) = 0;
    IoReleaseCancelSpinLock(*(undefined1 *)(param_1 + 0x25));
    *(undefined4 *)(iVar2 + 0x1c) = 0;
    *(undefined4 *)(iVar2 + 0x18) = 0xc0000120;
    (**(code **)(*(int *)((int)this + 0x1e0) + 0x20))(iVar2);
  }
  else {
    cVar3 = KeRemoveEntryDeviceQueue(local_c,param_1 + 0x40);
    if (cVar3 == '\0') {
      IoReleaseCancelSpinLock(*(undefined1 *)(iVar2 + 0x25));
    }
    else {
      IoReleaseCancelSpinLock(*(undefined1 *)(iVar2 + 0x25));
      *(undefined4 *)(iVar2 + 0x1c) = 0;
      FUN_00010798(&param_1,(int)this,0xc0000120);
    }
  }
  if (local_c != 0) {
    FUN_00010598(&local_c);
  }
  return;
}

