
void __thiscall FUN_00011018(int param_1,uint param_2)

{
  int iVar1;
  uint *puVar2;
  uint uVar3;
  uint uVar4;
  
  if (*(int *)(param_1 + 0x1463) == 0) {
    iVar1 = -0x3fffff5d;
  }
  else {
    iVar1 = KeWaitForSingleObject(*(int *)(param_1 + 0x1463),0,0,1,0);
  }
  uVar4 = param_2;
  if (iVar1 != 0) goto LAB_0001137f;
  uVar3 = *(uint *)(*(int *)(param_2 + 0x60) + 0xc);
  if (uVar3 < 0xcfdc2129) {
    if (uVar3 == 0xcfdc2128) {
      iVar1 = FUN_00011c36((void *)(param_1 + 0x1e0),param_2);
    }
    else if (uVar3 < 0x223045) {
      if (uVar3 == 0x223044) {
        iVar1 = FUN_00012d24((void *)(param_1 + 0x1e0),param_2);
      }
      else if (uVar3 == 0x222c00) {
        iVar1 = FUN_0001286e((void *)(param_1 + 0x1e0),param_2);
      }
      else if (uVar3 == 0x222c04) {
        iVar1 = FUN_000128bc((void *)(param_1 + 0x1e0),param_2);
      }
      else if (uVar3 == 0x223000) {
        iVar1 = FUN_00012ada((void *)(param_1 + 0x1e0),param_2);
      }
      else if (uVar3 == 0x223004) {
        iVar1 = FUN_00012a5e((void *)(param_1 + 0x1e0),param_2);
      }
      else if (uVar3 == 0x22303c) {
        iVar1 = FUN_00012cac((void *)(param_1 + 0x1e0),param_2);
      }
      else {
        if (uVar3 != 0x223040) goto LAB_000112a0;
        iVar1 = FUN_00012c18((void *)(param_1 + 0x1e0),param_2);
      }
    }
    else if (uVar3 == 0x223080) {
      iVar1 = FUN_000130ea((void *)(param_1 + 0x1e0),param_2);
    }
    else if (uVar3 == 0x223084) {
      iVar1 = FUN_000131b5((void *)(param_1 + 0x1e0),param_2);
    }
    else if (uVar3 == 0x223088) {
      iVar1 = FUN_00011f54((void *)(param_1 + 0x1e0),param_2);
    }
    else if (uVar3 == 0x223100) {
      iVar1 = FUN_00012988((void *)(param_1 + 0x1e0),param_2);
    }
    else {
      if (uVar3 == 0xcfdc2110) {
        FUN_00013ae2((void *)(param_1 + 0x1e0),param_2);
        goto LAB_000112e4;
      }
      if (uVar3 != 0xcfdc2124) goto LAB_000112a0;
      iVar1 = FUN_00011bdc((void *)(param_1 + 0x1e0),param_2);
    }
  }
  else if (uVar3 < 0xcfdc2191) {
    if (uVar3 == 0xcfdc2190) {
      iVar1 = FUN_00013a40((void *)(param_1 + 0x1e0),param_2);
    }
    else if (uVar3 == 0xcfdc212c) {
      iVar1 = FUN_00011c5e(param_2);
    }
    else if (uVar3 == 0xcfdc2130) {
      iVar1 = FUN_00011cff((void *)(param_1 + 0x1e0),param_2);
    }
    else if (uVar3 == 0xcfdc2138) {
      FUN_000141dc((void *)(param_1 + 0x1e0),param_2);
LAB_000112e4:
      iVar1 = *(int *)(uVar4 + 0x18);
    }
    else if (uVar3 == 0xcfdc2180) {
      iVar1 = FUN_000128f8((void *)(param_1 + 0x1e0),param_2);
    }
    else if (uVar3 == 0xcfdc2184) {
      iVar1 = 0;
      *(undefined4 *)(param_2 + 0x18) = 0;
      *(undefined4 *)(param_2 + 0x1c) = 0;
    }
    else {
      if (uVar3 != 0xcfdc218c) goto LAB_000112a0;
      iVar1 = FUN_00012b34((void *)(param_1 + 0x1e0),param_2);
    }
  }
  else if (uVar3 == 0xcfdc2194) {
    iVar1 = FUN_00012bae((void *)(param_1 + 0x1e0),param_2);
  }
  else if (uVar3 == 0xcfdc21c0) {
    iVar1 = FUN_00013954((void *)(param_1 + 0x1e0),param_2);
  }
  else if (uVar3 == 0xcfdc21c4) {
    iVar1 = FUN_0001272a((void *)(param_1 + 0x1e0),param_2);
  }
  else if (uVar3 == 0xcfdc21c8) {
    iVar1 = FUN_00012832(param_2);
  }
  else if (uVar3 == 0xcfdc2400) {
    iVar1 = FUN_00013a2e((void *)(param_1 + 0x1e0),param_2);
  }
  else {
    if (uVar3 == 0xcfdd219f) {
      FUN_000141f8((void *)(param_1 + 0x1e0),param_2);
      goto LAB_000112e4;
    }
LAB_000112a0:
    *(undefined4 *)(param_1 + 0x1452) = 3;
    uVar3 = param_2;
    puVar2 = FUN_00010750((uint *)(param_1 + 0x1436));
    puVar2 = FUN_0001919a(puVar2,uVar3);
    FUN_00010750(puVar2);
    *(undefined4 *)(uVar4 + 0x18) = 0xc000000d;
    *(undefined4 *)(uVar4 + 0x1c) = 0;
  }
  KeReleaseMutex(*(undefined4 *)(param_1 + 0x1463),0);
  if (iVar1 == 0x103) {
    return;
  }
  if (iVar1 != 0) {
    *(undefined4 *)(param_1 + 0x1452) = 3;
    puVar2 = FUN_00010750((uint *)(param_1 + 0x1436));
    puVar2 = FUN_0001076e(puVar2);
    puVar2 = FUN_0001919a(puVar2,uVar4);
    FUN_00010750(puVar2);
  }
LAB_0001137f:
  FUN_00010798(&param_2,param_1,iVar1);
  return;
}

