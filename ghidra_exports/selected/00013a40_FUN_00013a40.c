
undefined4 __thiscall FUN_00013a40(void *this,int param_1)

{
  undefined4 uVar1;
  undefined **ppuVar2;
  int iVar3;
  int *piVar4;
  int *piVar5;
  int local_28 [8];
  void *local_8;
  
  uVar1 = 0xc0000002;
  *(undefined4 *)(param_1 + 0x18) = 0xc0000002;
  if (DAT_0001cd08 == 0) {
    if ((*(int *)(*(int *)(param_1 + 0x60) + 8) == 0x1d) &&
       (*(int *)(*(int *)(param_1 + 0x60) + 4) == 0)) {
      piVar4 = *(int **)(param_1 + 0xc);
      piVar5 = local_28;
      local_8 = this;
      for (iVar3 = 7; iVar3 != 0; iVar3 = iVar3 + -1) {
        *piVar5 = *piVar4;
        piVar4 = piVar4 + 1;
        piVar5 = piVar5 + 1;
      }
      *(char *)piVar5 = (char)*piVar4;
      if (local_28[1] == 2) {
        if (local_28[2] == 0) {
          DAT_0001ce18 = DAT_0001ce18 & 0xfffffffd;
        }
        else {
          DAT_0001ce18 = DAT_0001ce18 | 2;
        }
        FUN_000107fe((void *)((int)this + 0x188),~local_28[2]);
        ppuVar2 = FUN_00010a88();
        (**(code **)*ppuVar2)(FUN_00012eae,local_8);
        *(undefined4 *)(param_1 + 0x1c) = 0;
        *(undefined4 *)(param_1 + 0x18) = 0;
        return 0;
      }
    }
    else {
      uVar1 = 0xc0000206;
    }
  }
  else {
    uVar1 = 0xc00000a3;
  }
  *(undefined4 *)(param_1 + 0x1c) = 0;
  *(undefined4 *)(param_1 + 0x18) = uVar1;
  return uVar1;
}

