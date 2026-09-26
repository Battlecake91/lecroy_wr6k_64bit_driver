
uint __thiscall FUN_000166a8(void *this,int param_1)

{
  undefined2 uVar1;
  uint uVar2;
  undefined2 extraout_var;
  undefined2 extraout_var_00;
  undefined2 extraout_var_01;
  undefined4 *puVar3;
  undefined2 extraout_var_02;
  undefined2 extraout_var_03;
  uint uVar4;
  undefined4 uVar5;
  
  uVar2 = (uint)*(byte *)(param_1 + 10);
  if (0x10 < uVar2) {
    if (uVar2 == 0x40) {
      FUN_00016066(this);
      uVar2 = FUN_00016490(this);
      uVar4 = uVar2;
      goto LAB_000167c0;
    }
switchD_000166cc_default:
    uVar4 = 2;
    goto LAB_000167c0;
  }
  if (uVar2 == 0x10) {
    uVar1 = FUN_000164b8(this,(char *)(param_1 + 0xb));
    uVar2 = CONCAT22(extraout_var_03,uVar1);
    uVar4 = uVar2;
    goto LAB_000167c0;
  }
  switch(uVar2) {
  case 0:
    FUN_0001634e(this,(char *)(param_1 + 0xb));
  case 4:
    uVar1 = FUN_00016414(this,param_1 + 0xb);
    uVar2 = CONCAT22(extraout_var_01,uVar1);
    uVar4 = uVar2;
    break;
  case 1:
    uVar1 = FUN_0001621a(this,(char *)(param_1 + 0xb));
    uVar2 = CONCAT22(extraout_var_00,uVar1);
    uVar4 = uVar2;
    break;
  case 2:
    uVar1 = FUN_000163b2(this,(char *)(param_1 + 0xb));
    uVar2 = CONCAT22(extraout_var,uVar1);
    uVar4 = uVar2;
    break;
  case 3:
    uVar2 = FUN_00016168(this,param_1,0x19);
    *(undefined4 *)((int)this + 0x196) = 10;
    uVar4 = uVar2;
    break;
  case 5:
    puVar3 = *(undefined4 **)((int)this + 0x15e);
    uVar4 = 4;
    if (puVar3 != (undefined4 *)0x0) {
      puVar3[9] = 7;
      WRITE_REGISTER_ULONG(*puVar3,7);
      puVar3 = *(undefined4 **)((int)this + 0x15e);
      puVar3[9] = 3;
      WRITE_REGISTER_ULONG(*puVar3,3);
LAB_0001676c:
      uVar4 = 0;
    }
    goto LAB_0001676e;
  case 6:
  case 7:
  case 8:
    uVar1 = FUN_00016066(this);
    uVar2 = CONCAT22(extraout_var_02,uVar1);
    uVar4 = uVar2;
    break;
  case 9:
    puVar3 = *(undefined4 **)((int)this + 0x15e);
    uVar4 = 4;
    if (puVar3 != (undefined4 *)0x0) {
      uVar5 = 3;
LAB_0001675f:
      puVar3[9] = uVar5;
      WRITE_REGISTER_ULONG(*puVar3,uVar5);
      goto LAB_0001676c;
    }
    goto LAB_0001676e;
  case 10:
    puVar3 = *(undefined4 **)((int)this + 0x15e);
    uVar4 = 4;
    if (puVar3 != (undefined4 *)0x0) {
      uVar5 = 2;
      goto LAB_0001675f;
    }
LAB_0001676e:
    uVar2 = FUN_00015a88(this,(short)uVar4);
    break;
  default:
    goto switchD_000166cc_default;
  }
LAB_000167c0:
  return CONCAT22((short)(uVar2 >> 0x10),(short)uVar4);
}

