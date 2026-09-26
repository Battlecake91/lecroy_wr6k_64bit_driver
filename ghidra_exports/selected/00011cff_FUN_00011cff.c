
undefined4 __thiscall FUN_00011cff(void *this,int param_1)

{
  int iVar1;
  uint *puVar2;
  uint uVar3;
  undefined4 uVar4;
  byte *pbVar5;
  
  pbVar5 = *(byte **)(param_1 + 0xc);
  if (pbVar5 == (byte *)0x0) {
    *(undefined4 *)(param_1 + 0x18) = 0xc000000d;
    *(undefined4 *)((int)this + 0x1272) = 3;
    puVar2 = FUN_00010750((uint *)((int)this + 0x1256));
    FUN_00010750(puVar2);
    uVar4 = *(undefined4 *)(param_1 + 0x18);
  }
  else {
    iVar1 = *(int *)(*(int *)(param_1 + 0x60) + 8);
    if (iVar1 == 0) {
      *(undefined4 *)((int)this + 0x1272) = 1;
      puVar2 = FUN_00010750((uint *)((int)this + 0x1256));
      FUN_00010750(puVar2);
      uVar4 = 0xc0000206;
    }
    else {
      uVar3 = READ_REGISTER_ULONG(*(undefined4 *)((int)this + 0x318));
      for (; iVar1 != 0; iVar1 = iVar1 + -1) {
        uVar3 = uVar3 ^ ((uint)*pbVar5 << 8 ^ uVar3) & 0xe000;
        FUN_000107fe((undefined4 *)((int)this + 0x318),uVar3);
        pbVar5 = pbVar5 + 1;
      }
      *(undefined4 *)(param_1 + 0x18) = 0;
      *(undefined4 *)(param_1 + 0x1c) = 0;
      uVar4 = 0;
    }
  }
  return uVar4;
}

