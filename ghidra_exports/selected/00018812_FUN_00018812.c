
int __thiscall FUN_00018812(void *this,uint param_1)

{
  uint uVar1;
  uint uVar2;
  int iVar3;
  
  iVar3 = 0;
  uVar1 = FUN_000187f2(this,param_1);
  uVar2 = 0;
  if ((uVar1 & 1) == 0) {
    if (param_1 != 0) {
      do {
        uVar1 = FUN_000187f2(this,uVar2);
        if ((uVar1 & 1) == 0) {
          iVar3 = iVar3 + 1;
        }
        uVar2 = uVar2 + 1;
      } while (uVar2 < param_1);
    }
  }
  else if (param_1 != 0) {
    do {
      uVar1 = FUN_000187f2(this,uVar2);
      if ((uVar1 & 1) != 0) {
        iVar3 = iVar3 + 1;
      }
      uVar2 = uVar2 + 1;
    } while (uVar2 < param_1);
  }
  return iVar3;
}

