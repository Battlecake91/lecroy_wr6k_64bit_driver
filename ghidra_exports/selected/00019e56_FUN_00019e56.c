
int __thiscall
FUN_00019e56(void *this,undefined4 param_1,undefined4 param_2,int param_3,int param_4)

{
  undefined4 *puVar1;
  int iVar2;
  undefined1 *puVar3;
  void *local_8;
  
  local_8 = this;
  puVar1 = (undefined4 *)ExAllocatePoolWithTag(0,0x10,0x206d6457);
  if (puVar1 == (undefined4 *)0x0) {
    iVar2 = -0x3fffff66;
  }
  else {
    puVar1[2] = param_3;
    *puVar1 = this;
    puVar1[1] = 0;
    puVar1[3] = param_4;
    local_8 = (void *)0x0;
    if ((char)param_1 == '\0') {
      if (*(int *)((int)this + 0x1ac) != 0) {
        return -0x7fffffef;
      }
      param_2 = *(undefined4 *)((int)this + 400);
      puVar3 = &LAB_00019c64;
    }
    else {
      puVar3 = &LAB_00019c80;
    }
    iVar2 = PoRequestPowerIrp(*(undefined4 *)((int)this + 0x34),param_1,param_2,puVar3,puVar1,
                              &local_8);
    if (((char)param_1 == '\0') && (iVar2 == 0x103)) {
      *(void **)((int)this + 0x1ac) = local_8;
    }
  }
  return iVar2;
}

