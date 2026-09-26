
uint __thiscall FUN_00016a66(void *this,int param_1)

{
  undefined2 uVar1;
  uint uVar2;
  uint uVar3;
  undefined2 extraout_var;
  uint uVar4;
  undefined2 extraout_var_00;
  undefined2 extraout_var_01;
  
  uVar2 = (uint)*(byte *)(param_1 + 10);
  uVar4 = 2;
  if (uVar2 < 0x86) {
    if (uVar2 == 0x85) {
      uVar4 = FUN_00016962(this,param_1 + 0xb);
      uVar3 = FUN_00016168(this,param_1,1);
      uVar4 = uVar4 | uVar3;
      goto switchD_00016b14_caseD_8a;
    }
    if (uVar2 < 0x78) {
      if ((uVar2 != 0x77) && (uVar2 != 0x40)) {
        if (uVar2 == 0x42) {
          uVar3 = FUN_00015acc(this,(byte *)(param_1 + 0xb));
          uVar4 = uVar3;
          goto switchD_00016b14_caseD_8a;
        }
        if (uVar2 == 0x4a) goto switchD_00016b14_caseD_86;
        if (((uVar2 != 0x50) && (uVar2 != 0x66)) && (uVar3 = uVar2 - 0x76, uVar3 != 0))
        goto switchD_00016b14_caseD_8a;
      }
    }
    else if ((((uVar2 != 0x80) && (uVar2 != 0x81)) && (uVar2 != 0x82)) && (uVar2 != 0x83)) {
      uVar3 = uVar2 - 0x84;
      if (uVar3 != 0) goto switchD_00016b14_caseD_8a;
      goto switchD_00016b14_caseD_86;
    }
switchD_00016b14_caseD_89:
    uVar1 = FUN_00016066(this);
    uVar3 = CONCAT22(extraout_var,uVar1);
    uVar4 = uVar3;
  }
  else {
    uVar3 = 0;
    switch(uVar2) {
    case 0x88:
      *(ushort *)((int)this + 9) = *(ushort *)((int)this + 9) & ~*(ushort *)(param_1 + 0xc);
      if ((*(short *)(param_1 + 0xc) == 0x80) || (*(short *)(param_1 + 0xc) == 0x800)) {
        uVar3 = FUN_00015a88(this,0);
        uVar4 = 0;
        break;
      }
    case 0x86:
    case 0x87:
    case 0x96:
    case 0x97:
    case 0xa1:
    case 0xa2:
switchD_00016b14_caseD_86:
      uVar3 = FUN_00016168(this,param_1,1);
      uVar4 = uVar3;
      break;
    case 0x89:
      goto switchD_00016b14_caseD_89;
    case 0x90:
      uVar3 = FUN_00015e80(this,(ushort *)(param_1 + 0xb));
      uVar4 = uVar3;
      break;
    case 0x91:
      uVar1 = FUN_00015f7c(this,(char *)(param_1 + 0xb));
      uVar3 = CONCAT22(extraout_var_00,uVar1);
      uVar4 = uVar3;
      break;
    case 0x92:
      uVar1 = FUN_0001600e(this,(char *)(param_1 + 0xb));
      uVar3 = CONCAT22(extraout_var_01,uVar1);
      uVar4 = uVar3;
      break;
    case 0xa0:
      uVar3 = FUN_00015fd8(this,param_1 + 0xb);
      uVar4 = uVar3;
    }
  }
switchD_00016b14_caseD_8a:
  return CONCAT22((short)(uVar3 >> 0x10),(short)uVar4);
}

