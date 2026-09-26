
uint __thiscall FUN_000165a6(void *this,int param_1,int *param_2)

{
  byte bVar1;
  ushort uVar2;
  undefined2 uVar3;
  undefined2 extraout_var;
  undefined2 extraout_var_00;
  uint uVar4;
  undefined2 extraout_var_01;
  undefined2 extraout_var_02;
  undefined2 extraout_var_03;
  undefined2 extraout_var_04;
  
  bVar1 = *(byte *)(param_1 + 10);
  if (0x83 < bVar1) {
    switch(bVar1) {
    case 0x90:
    case 0x91:
    case 0x96:
    case 0x97:
    case 0x99:
      goto switchD_0001663d_caseD_90;
    case 0x92:
      uVar3 = FUN_00015dea(this,(char *)(param_1 + 0xb));
      return CONCAT22(extraout_var_01,uVar3);
    default:
      return 2;
    case 0xa0:
      uVar3 = FUN_00015b64(this,(char *)(param_1 + 0xb));
      return CONCAT22(extraout_var_02,uVar3);
    case 0xa1:
      uVar3 = FUN_00015bce(this);
      return CONCAT22(extraout_var_03,uVar3);
    case 0xa2:
      uVar3 = FUN_00015c26(this);
      return CONCAT22(extraout_var_04,uVar3);
    }
  }
  if (bVar1 != 0x83) {
    if (bVar1 < 0x61) {
      if ((bVar1 != 0x60) && (bVar1 != 0x40)) {
        if (bVar1 == 0x42) {
          uVar3 = FUN_00015c7e(this,(byte *)(param_1 + 0xb));
          return CONCAT22(extraout_var_00,uVar3);
        }
        if (bVar1 != 0x4a) {
          if (bVar1 < 0x50) {
            return 2;
          }
          if (0x51 < bVar1) {
            return 2;
          }
          uVar2 = FUN_000160dc(this,param_1 + 0xb,param_2);
          return CONCAT22(extraout_var,uVar2);
        }
switchD_0001663d_caseD_90:
        uVar4 = FUN_00015db8(this,param_1,param_2,1);
        return uVar4;
      }
    }
    else if ((bVar1 != 0x70) && (bVar1 != 0x80)) {
      if ((bVar1 != 0x81) && (bVar1 != 0x82)) {
        return 2;
      }
      goto switchD_0001663d_caseD_90;
    }
  }
  FUN_00015a88(this,0x10);
  return 0x10;
}

