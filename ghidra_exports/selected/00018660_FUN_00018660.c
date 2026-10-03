
undefined4 __thiscall FUN_00018660(void *this,uint *param_1,int param_2,undefined1 param_3)

{
  void *local_8;
  
  local_8 = this;
  if (*(int *)((int)this + 4) != 0) {
    FUN_00018648((int)this);
  }
  *(undefined4 *)((int)this + 0x24) = 0;
  *(undefined4 *)((int)this + 0x14) = 0xffffffff;
  *(undefined4 *)((int)this + 0x18) = 0xffffffff;
  *(undefined4 *)((int)this + 0x1c) = 0xffffffff;
  *(undefined4 *)((int)this + 0x20) = 0xffffffff;
  if ((param_1 != (uint *)0x0) &&
     (FUN_0001dc76(&local_8,param_1,'\x02',param_2), local_8 != (void *)0x0)) {
    *(uint *)((int)this + 0x28) = *(byte *)((int)local_8 + 2) & 1;
    *(undefined1 *)((int)this + 0xc) = *(undefined1 *)((int)local_8 + 4);
    *(undefined4 *)((int)this + 8) = *(undefined4 *)((int)local_8 + 8);
    *(undefined4 *)((int)this + 0x10) = *(undefined4 *)((int)local_8 + 0xc);
    *(undefined1 *)((int)this + 0xd) = *(undefined1 *)((int)this + 0xc);
    *(bool *)((int)this + 0x2c) = *(char *)((int)local_8 + 1) != '\x01';
    *(undefined1 *)((int)this + 0x2d) = param_3;
    return 0;
  }
  return 0xc000008a;
}

