
int __thiscall FUN_000158ee(void *this,undefined4 param_1,undefined4 param_2)

{
  void *this_00;
  undefined4 *puVar1;
  
  *(undefined4 *)((int)this + 0xd) = 0;
  *(undefined4 *)((int)this + 0x11) = 0;
  FUN_000174f2((int)this + 0x31);
  FUN_00015356((int)this + 0x13a);
  *(undefined4 *)((int)this + 0x15) = param_1;
  *(undefined4 *)((int)this + 0x182) = 0;
  *(undefined4 *)((int)this + 0x19a) = 5;
  *(undefined4 *)((int)this + 0x19e) = 0;
  *(undefined4 *)((int)this + 0x19) = param_2;
  *(undefined4 *)((int)this + 0x1d) = 0;
  *(undefined1 *)((int)this + 0x23) = 0;
  *(undefined2 *)((int)this + 0x21) = 0;
  *(undefined2 *)((int)this + 9) = 0;
  *(undefined2 *)((int)this + 0xb) = 0x20;
  *(undefined4 *)((int)this + 0x25) = 0;
  *(undefined4 *)((int)this + 0x29) = 0;
  *(undefined4 *)((int)this + 0x2d) = 0;
  *(undefined4 *)((int)this + 4) = 0;
  *(undefined1 *)((int)this + 0x24) = 0;
  *(undefined4 *)((int)this + 0x15e) = 0;
  *(undefined4 *)((int)this + 0x162) = 0;
  *(undefined4 *)((int)this + 0x166) = 0;
  *(undefined4 *)((int)this + 0x16a) = 0;
  *(undefined4 *)((int)this + 0x16e) = 0;
  *(undefined4 *)((int)this + 0x172) = 0;
  *(undefined4 *)((int)this + 0x196) = 5;
  this_00 = (void *)FUN_00010380(0x40);
  if (this_00 == (void *)0x0) {
    puVar1 = (undefined4 *)0x0;
  }
  else {
    puVar1 = FUN_000157c4(this_00,0);
  }
  *(undefined4 **)((int)this + 0x186) = puVar1;
  (**(code **)*puVar1)(0,0);
  *(undefined4 *)((int)this + 0x18a) = 0;
  *(undefined4 *)((int)this + 0x18e) = 0;
  *(undefined4 *)((int)this + 0x192) = 0;
  return (int)this;
}

