
void __thiscall FUN_00019f54(void *this,int param_1)

{
  int iVar1;
  
  if (param_1 == 0) {
    for (iVar1 = FUN_00019bc6((int *)((int)this + 0x1d0)); iVar1 != 0;
        iVar1 = FUN_00019be4((int *)((int)this + 0x1d0),iVar1)) {
      IoSetDeviceInterfaceState(iVar1 + 4,1);
    }
  }
  else {
    IoSetDeviceInterfaceState(param_1,1);
  }
  return;
}

