
undefined4 * __thiscall
FUN_0001d868(void *this,undefined4 param_1,undefined4 param_2,undefined4 param_3)

{
  void *local_c;
  void *pvStack_8;
  
  *(undefined4 *)this = 0;
  *(undefined4 *)((int)this + 4) = param_2;
  *(undefined4 *)((int)this + 0xc) = param_3;
  *(undefined1 *)((int)this + 0x18) = 1;
  local_c = this;
  pvStack_8 = this;
  RtlInitUnicodeString(&local_c,param_1);
  FUN_0001d7fa(this,(ushort *)&local_c);
  return this;
}

