
int FUN_0001d418(void *param_1,short *param_2,short *param_3,undefined4 param_4)

{
  ushort uVar1;
  int iVar2;
  int iVar3;
  
  uVar1 = *(ushort *)((int)param_1 + 2);
  iVar2 = FUN_0001bcce(param_2);
  iVar3 = FUN_0001bcce(param_3);
  iVar2 = iVar2 + iVar3 + (2 - (uint)(uVar1 >> 1));
  if ((0 < iVar2) && (iVar2 = FUN_0001bda0(param_1,(short)iVar2,param_4), iVar2 < 0)) {
    return iVar2;
  }
  FUN_0001bd6a(param_1,param_2);
  FUN_000184c0(param_1,param_3);
  return 0;
}

