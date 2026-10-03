
undefined4 * __thiscall FUN_00010b3c(void *this,undefined4 param_1,undefined4 param_2)

{
  undefined4 *this_00;
  
  FUN_0001a0b6(this,param_1,0x1cc88);
  FUN_00014212((undefined4 *)((int)this + 0x1e0));
  *(undefined4 *)((int)this + 0x1e0) = &PTR_FUN_0001c62c;
  this_00 = (undefined4 *)((int)this + 0x14a5);
  *(undefined ***)this = &PTR_LAB_0001c500;
  FUN_0001b9fa(this_00);
  *(undefined4 *)((int)this + 0x14b5) = 0;
  *(undefined4 *)((int)this + 0x14c5) = 0;
  *(undefined1 *)((int)this + 0x14cd) = 0;
  *(undefined4 *)((int)this + 0x14d5) = 0;
  *(undefined4 *)((int)this + 0x14e5) = 0;
  *(undefined1 *)((int)this + 0x14ed) = 0;
  *(undefined4 *)((int)this + 0x14f5) = 0;
  *(undefined4 *)((int)this + 0x1505) = 0;
  *(undefined1 *)((int)this + 0x150d) = 0;
  if (-1 < *(int *)((int)this + 0x24)) {
    *(undefined4 *)((int)this + 0x14a1) = param_2;
    FUN_0001b98e(this_00,(int)this,param_1);
    *(undefined4 **)((int)this + 0x30) = this_00;
    FUN_000194ba((int)this);
    FUN_00019734(this,0);
    *(uint *)((int)this + 0x140) = *(uint *)((int)this + 0x140) & 0xfffffff4;
    *(uint *)((int)this + 0x118) = *(uint *)((int)this + 0x118) & 0xfffffff4;
    FUN_00019ee6((int)this);
    FUN_00019ee6((int)this);
    FUN_00019ee6((int)this);
    FUN_00019ee6((int)this);
  }
  return this;
}

