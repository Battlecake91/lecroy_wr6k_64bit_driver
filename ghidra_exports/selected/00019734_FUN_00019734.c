
void __thiscall FUN_00019734(void *this,uint param_1)

{
  uint uVar1;
  
  uVar1 = param_1 & 1;
  *(uint *)((int)this + 0x110) = *(uint *)((int)this + 0x110) & 0xffffffa9 | uVar1 << 2 | 0xa9;
  *(uint *)((int)this + 0x128) = *(uint *)((int)this + 0x128) & 0xffffffe3 | 99;
  *(uint *)((int)this + 0x118) = *(uint *)((int)this + 0x118) | 0x7f;
  *(uint *)((int)this + 0x140) = *(uint *)((int)this + 0x140) | 0x7f;
  *(uint *)((int)this + 0x138) = *(uint *)((int)this + 0x138) & 0xfffffff3 | uVar1 << 2 | 3;
  *(undefined4 *)((int)this + 0x114) = 4;
  *(uint *)((int)this + 0x150) =
       (uVar1 | uVar1 * 2) << 3 | *(uint *)((int)this + 0x150) & 0xffffffc0 | uVar1 |
       (uVar1 | uVar1 << 4) << 1;
  return;
}

