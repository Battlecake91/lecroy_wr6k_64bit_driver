
undefined4 __thiscall FUN_00013f70(void *this)

{
  undefined4 *in_stack_00000110;
  
  FUN_00013230(this,*(int *)((int)this + 0xc) + 1,(undefined4 *)&stack0x00000004);
  if (in_stack_00000110 != (undefined4 *)0x0) {
    if (*(int *)((int)this + 0x14) < 0) {
      *in_stack_00000110 = 0xffffffff;
    }
    else {
      *in_stack_00000110 = *(undefined4 *)((int)this + 0xc);
    }
  }
  return *(undefined4 *)((int)this + 0x14);
}

