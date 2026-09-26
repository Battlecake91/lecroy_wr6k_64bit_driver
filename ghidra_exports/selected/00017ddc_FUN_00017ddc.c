
int __thiscall FUN_00017ddc(void *this,uint param_1)

{
  FUN_00017a16((undefined4 *)((int)this + 8));
  FUN_00017a16((undefined4 *)((int)this + 0x24));
  FUN_00017a16((undefined4 *)((int)this + 0x40));
  FUN_00017a16((undefined4 *)((int)this + 0x5c));
  *(undefined1 *)((int)this + 0x78) = 0;
  *(undefined2 *)((int)this + 0x7a) = 0x400;
  FUN_00017a98((undefined4 *)((int)this + 0x24),param_1,1,0);
  FUN_00017a98((undefined4 *)((int)this + 0x40),param_1,1,0);
  FUN_00017a98((void *)((int)this + 0x5c),param_1,1,0);
  FUN_00017a36((undefined4 *)((int)this + 8),param_1,1,0);
  *(undefined4 *)((int)this + 4) = 0xffffffff;
  return (int)this;
}

