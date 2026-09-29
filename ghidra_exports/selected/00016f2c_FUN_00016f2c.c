
int __thiscall FUN_00016f2c(void *this,byte *param_1,int param_2)

{
  byte bVar1;
  byte *pbVar2;
  int iVar3;
  
  pbVar2 = param_1;
  iVar3 = -0x3fffffff;
  if (((param_1 != (byte *)0x0) && (param_2 != 0)) && (iVar3 = FUN_00016cb0(this), iVar3 == 0)) {
    if (*(char *)((int)this + 0x18) == '\0') {
      iVar3 = -0x3fffff40;
    }
    else {
      FUN_00016cf0(this,0xcc);
      FUN_00016cf0(this,0xf0);
      FUN_00016cf0(this,0);
      FUN_00016cf0(this,0);
      iVar3 = 0;
      if (param_2 != 0) {
        param_1 = (byte *)param_2;
        do {
          bVar1 = FUN_00016d22(this);
          *pbVar2 = bVar1;
          pbVar2 = pbVar2 + 1;
          param_1 = param_1 + -1;
        } while (param_1 != (byte *)0x0);
      }
    }
  }
  return iVar3;
}

