
undefined2 __thiscall FUN_00016414(void *this,uint param_1)

{
  uint uVar1;
  undefined2 uVar2;
  int iVar3;
  
  if (param_1 == 0) {
    uVar2 = 8;
  }
  else {
    uVar1 = DAT_0001ce14 >> 2;
    FUN_00016074(this,'\0');
    iVar3 = *(int *)(*(int *)((int)this + 0x29) + 0x10) + 0x80;
    if (*(ushort *)(param_1 + 1) != 0) {
      param_1 = (uint)*(ushort *)(param_1 + 1);
      do {
        WRITE_REGISTER_ULONG(iVar3,1);
        WRITE_REGISTER_ULONG(iVar3,0);
        param_1 = param_1 - 1;
      } while (param_1 != 0);
    }
    if ((uVar1 & 1) != 0) {
      FUN_00016074(this,'\x01');
    }
    uVar2 = 0;
  }
  FUN_00015a88(this,uVar2);
  return uVar2;
}

