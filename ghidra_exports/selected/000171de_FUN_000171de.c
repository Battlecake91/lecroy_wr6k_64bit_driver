
int __thiscall FUN_000171de(void *this,undefined4 param_1,int param_2,char param_3)

{
  int iVar1;
  void *this_00;
  undefined8 local_c;
  
  local_c._0_4_ = this;
  local_c._4_4_ = this;
  FUN_000107fe((void *)((int)this + 0x58),param_1);
  *(int *)((int)this + 0xfc) = param_2;
  FUN_000107fe((void *)((int)this + 0x80),*(undefined4 *)(*(int *)((int)this + 0x100) + 0x18));
  *(undefined4 *)((int)this + 300) = *(undefined4 *)(*(int *)((int)this + 0x100) + 0x18);
  KeResetEvent(*(int *)((int)this + 0x100) + 0x24);
  (**(code **)(*(int *)this + 0x14))();
  FUN_000107fe((void *)((int)this + 4),*(undefined4 *)((int)this + 0x54));
  this_00 = (void *)((int)this + 0x2c);
  if (param_3 == '\0') {
    this_00 = (void *)((int)this + 0xd0);
  }
  FUN_000107fe(this_00,*(undefined4 *)((int)this + 0xfc));
  local_c = RtlConvertLongToLargeInteger(0xfd050f80);
  iVar1 = KeWaitForSingleObject(*(int *)((int)this + 0x100) + 0x24,0,0,0,&local_c);
  (**(code **)(*(int *)this + 0x18))();
  if ((iVar1 != 0) && (iVar1 == 0x102)) {
    iVar1 = -0x3fffff4b;
  }
  return iVar1;
}

