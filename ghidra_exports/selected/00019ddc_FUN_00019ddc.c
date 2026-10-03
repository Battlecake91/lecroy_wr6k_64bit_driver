
int __thiscall FUN_00019ddc(void *this,undefined4 param_1,undefined4 param_2,int param_3)

{
  undefined4 *puVar1;
  int iVar2;
  int local_24;
  undefined4 local_20;
  
  FUN_0001dd1e(&local_24,0,0);
  puVar1 = (undefined4 *)ExAllocatePoolWithTag(0,0x10,0x206d6457);
  if (puVar1 == (undefined4 *)0x0) {
    iVar2 = -0x3fffff66;
  }
  else {
    puVar1[1] = &local_24;
    *puVar1 = this;
    puVar1[2] = param_3;
    iVar2 = PoRequestPowerIrp(*(undefined4 *)((int)this + 0x34),param_1,param_2,&LAB_00019c80,puVar1
                              ,0);
    if (iVar2 == 0x103) {
      iVar2 = KeWaitForSingleObject(local_20,0,0,1,0);
    }
  }
  FUN_00018870(&local_24);
  return iVar2;
}

