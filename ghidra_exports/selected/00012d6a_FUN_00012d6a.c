
int __thiscall
FUN_00012d6a(void *this,int param_1,uint param_2,uint *param_3,char param_4,uint param_5)

{
  char cVar1;
  uint uVar2;
  undefined4 local_8;
  
  *param_3 = 0;
  local_8 = -0x3ffffffe;
  if (param_5 == 0) {
    uVar2 = 0x400;
    if (param_2 < 0x400) {
      uVar2 = param_2;
    }
    param_5 = param_2 / uVar2;
  }
  FUN_000120dc((int)this);
  if (param_4 == '\0') {
    FUN_00017cdc((void *)((int)this + 0x1086),param_1);
    cVar1 = '\x01';
  }
  else {
    cVar1 = FUN_00017d20((void *)((int)this + 0x1086),param_1,param_2);
  }
  if (cVar1 != '\0') {
    local_8 = FUN_00017478(this,param_5,'\x01');
  }
  uVar2 = READ_REGISTER_ULONG(*(undefined4 *)((int)this + 0x2a0));
  if ((uVar2 & 1) != 0) {
    *(undefined4 *)((int)this + 0x1fc) = 0;
    WRITE_REGISTER_ULONG(*(undefined4 *)((int)this + 0x1d8),0);
  }
  READ_REGISTER_ULONG(*(undefined4 *)((int)this + 0x160));
  *param_3 = param_2;
  return local_8;
}

