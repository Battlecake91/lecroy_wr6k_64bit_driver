
undefined4 * __thiscall FUN_0001be34(void *this,undefined4 param_1,undefined4 param_2)

{
  undefined4 uVar1;
  void *local_c;
  void *pvStack_8;
  
  *(undefined4 *)((int)this + 4) = 0;
  *(undefined4 *)((int)this + 8) = 0;
  local_c = this;
  pvStack_8 = this;
  RtlInitUnicodeString(&local_c,param_1);
  uVar1 = IoGetDeviceObjectPointer
                    (&local_c,param_2,(undefined4 *)((int)this + 8),(undefined4 *)((int)this + 4));
  *(undefined4 *)this = uVar1;
  return this;
}

