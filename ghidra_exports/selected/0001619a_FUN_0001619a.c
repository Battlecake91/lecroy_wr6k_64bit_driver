
void __thiscall
FUN_0001619a(void *this,ushort *param_1,uint param_2,undefined2 *param_3,uint param_4,int *param_5)

{
  void *this_00;
  int iVar1;
  undefined8 local_c;
  
  *param_5 = 0;
  this_00 = (void *)((int)this + 0x31);
  local_c._0_4_ = this;
  local_c._4_4_ = this;
  KeResetEvent(this_00);
  FUN_000160a8(this,'\x01');
  FUN_000176e6(this_00,param_1,param_2,1);
  local_c = RtlConvertLongToLargeInteger(*(int *)((int)this + 0x196) * -10000000);
  *(int *)((int)this + 0x196) = *(int *)((int)this + 0x19a);
  iVar1 = KeWaitForSingleObject(this_00,0,0,0,&local_c);
  if (iVar1 == 0) {
    FUN_00017578(this_00,param_3,param_4,param_5);
  }
  return;
}

