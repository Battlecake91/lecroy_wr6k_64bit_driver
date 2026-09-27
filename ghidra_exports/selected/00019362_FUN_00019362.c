
undefined4 * __thiscall
FUN_00019362(void *this,undefined4 param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4,
            int param_5,undefined4 param_6)

{
  void *local_c;
  void *pvStack_8;
  
  *(undefined4 *)this = param_2;
  *(undefined4 *)((int)this + 4) = param_3;
  *(undefined4 *)((int)this + 8) = param_4;
  *(undefined4 *)((int)this + 0xc) = 0;
  *(undefined4 *)((int)this + 0x10) = 0;
  *(undefined4 *)((int)this + 0x14) = 0;
  *(undefined4 *)((int)this + 0x1c) = 0;
  *(undefined4 *)((int)this + 0x20) = 0;
  *(undefined1 *)((int)this + 0x24) = 0;
  *(undefined1 *)((int)this + 0x18) = 1;
  local_c = this;
  pvStack_8 = this;
  RtlInitAnsiString(&local_c,param_1);
  FUN_000192ac(this,(ushort *)&local_c,param_5,param_6);
  return this;
}

