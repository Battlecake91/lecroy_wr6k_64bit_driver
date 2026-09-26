
uint __thiscall FUN_00016c14(void *this,ushort *param_1,int *param_2)

{
  ushort uVar1;
  undefined4 uVar2;
  
  if ((param_1 != (ushort *)0x0) && (param_2 != (int *)0x0)) {
    uVar1 = param_1[3];
    if (uVar1 == 0x85fb) {
      uVar2 = FUN_000169b4(this,param_1,param_2);
LAB_00016c5e:
      return CONCAT31((int3)((uint)uVar2 >> 8),1);
    }
    if (uVar1 == 0xa5fb) {
      param_1 = (ushort *)FUN_00016bba(this,(int)param_1,param_2);
    }
    else if (uVar1 == 0xc5fb) {
      uVar2 = FUN_00016594((int)param_1,param_2);
      goto LAB_00016c5e;
    }
  }
  return (uint)param_1 & 0xffffff00;
}

