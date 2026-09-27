
undefined4 __thiscall FUN_00012a5e(void *this,int param_1)

{
  uint uVar1;
  undefined4 uVar2;
  uint uVar3;
  uint *puVar4;
  uint *puVar5;
  
  *(undefined4 *)(param_1 + 0x18) = 0xc0000002;
  puVar4 = *(uint **)((int)this + 0x11ea);
  if (puVar4 == (uint *)0x0) {
    uVar1 = 0xffffffff;
  }
  else {
    uVar1 = *puVar4;
  }
  uVar3 = *(uint *)(*(int *)(param_1 + 0x60) + 4);
  if (uVar3 == 4) {
    if (uVar1 != 0xffffffff) {
      **(uint **)(param_1 + 0xc) = uVar1;
      *(undefined4 *)(param_1 + 0x18) = 0;
      *(undefined4 *)(param_1 + 0x1c) = 4;
      return 0;
    }
    uVar2 = 0xc000000d;
  }
  else {
    if (uVar3 == uVar1) {
      puVar5 = *(uint **)(param_1 + 0xc);
      for (uVar3 = uVar1 >> 2; uVar3 != 0; uVar3 = uVar3 - 1) {
        *puVar5 = *puVar4;
        puVar4 = puVar4 + 1;
        puVar5 = puVar5 + 1;
      }
      for (uVar3 = uVar1 & 3; uVar3 != 0; uVar3 = uVar3 - 1) {
        *(char *)puVar5 = (char)*puVar4;
        puVar4 = (uint *)((int)puVar4 + 1);
        puVar5 = (uint *)((int)puVar5 + 1);
      }
      *(undefined4 *)(param_1 + 0x18) = 0;
      *(uint *)(param_1 + 0x1c) = uVar1;
      return 0;
    }
    uVar2 = 0xc0000206;
  }
  *(undefined4 *)(param_1 + 0x1c) = 0;
  *(undefined4 *)(param_1 + 0x18) = uVar2;
  return uVar2;
}

