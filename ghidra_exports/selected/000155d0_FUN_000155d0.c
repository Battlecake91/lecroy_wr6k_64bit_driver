
uint __thiscall FUN_000155d0(void *this,byte param_1,byte param_2)

{
  uint uVar1;
  
  uVar1 = (uint)param_1;
  if (uVar1 == 0) {
    uVar1 = (param_2 & 1) << 0xe | *(uint *)((int)this + 0x10) & 0xffffbc1f;
  }
  else if (uVar1 == 1) {
    uVar1 = (param_2 & 1) << 0xf | *(uint *)((int)this + 0x10) & 0xffff7c3f | 0x20;
  }
  else if (uVar1 == 2) {
    uVar1 = (param_2 & 1) << 0x10 | *(uint *)((int)this + 0x10) & 0xfffefd5f | 0x140;
  }
  else if (uVar1 == 3) {
    uVar1 = (param_2 & 1) << 0x11 | *(uint *)((int)this + 0x10) & 0xfffdfd7f | 0x160;
  }
  else if (uVar1 == 4) {
    uVar1 = (param_2 & 1) << 0x12 | *(uint *)((int)this + 0x10) & 0xfffbfd9f | 0x180;
  }
  else if (uVar1 == 0xc) {
    uVar1 = ((uint)param_2 << 0xd ^ *(uint *)((int)this + 0x10)) & 0x2000 ^
            *(uint *)((int)this + 0x10) | 0x300;
  }
  else {
    if (uVar1 - 0xe != 0) {
      return uVar1 - 0xe & 0xffffff00;
    }
    uVar1 = (param_2 & 1) << 0xc | *(uint *)((int)this + 0x10) & 0xffffeebf | 0x2a0;
  }
  *(uint *)((int)this + 0x10) = uVar1;
  return CONCAT31((int3)(uVar1 >> 8),1);
}

