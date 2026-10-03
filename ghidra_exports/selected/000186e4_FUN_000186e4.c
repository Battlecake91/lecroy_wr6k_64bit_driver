
void __thiscall
FUN_000186e4(void *this,int param_1,int param_2,char param_3,int param_4,char param_5)

{
  int iVar1;
  undefined4 local_8;
  
  local_8 = 0;
  if (*(char *)((int)this + 0x18) != '\0') {
    FUN_000106a0((int)this);
  }
  *(int *)((int)this + 0x14) = param_4;
  *(int *)((int)this + 8) = param_1;
  *(int *)((int)this + 0xc) = param_2;
  *(uint *)((int)this + 4) = (uint)(param_3 != '\0');
  if ((param_3 != '\0') == 0) {
    if (param_5 == '\0') {
      *(undefined4 *)((int)this + 0x10) = 0xffffffff;
    }
    else {
      iVar1 = MmMapIoSpace(param_1,param_2,param_4,0);
      *(int *)((int)this + 0x10) = iVar1;
      if (iVar1 == 0) {
        local_8 = -0x3fffff66;
      }
    }
  }
  else {
    *(undefined4 *)((int)this + 0x10) = *(undefined4 *)((int)this + 8);
  }
  *(bool *)((int)this + 0x18) = -1 < local_8;
  *(int *)this = local_8;
  return;
}

