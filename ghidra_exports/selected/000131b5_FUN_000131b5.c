
int __thiscall FUN_000131b5(void *this,int param_1)

{
  uint uVar1;
  int iVar2;
  uint *puVar3;
  
  if (*(byte **)(param_1 + 0xc) == (byte *)0x0) {
    iVar2 = -0x3ffffff3;
    *(undefined4 *)(param_1 + 0x18) = 0xc000000d;
  }
  else {
    uVar1 = *(uint *)(*(int *)(param_1 + 0x60) + 4);
    if ((uVar1 == 0) || (0x200 < uVar1)) {
      *(undefined4 *)((int)this + 0x1272) = 3;
      puVar3 = FUN_00010750((uint *)((int)this + 0x1256));
      FUN_00010750(puVar3);
      iVar2 = -0x3ffffdfa;
    }
    else {
      iVar2 = FUN_00016f2c((void *)((int)this + 0x12ab),*(byte **)(param_1 + 0xc),uVar1);
      if (iVar2 == 0) {
        *(undefined4 *)(param_1 + 0x1c) = *(undefined4 *)(*(int *)(param_1 + 0x60) + 4);
      }
      else {
        *(undefined4 *)(param_1 + 0x1c) = 0;
      }
      *(int *)(param_1 + 0x18) = iVar2;
    }
  }
  return iVar2;
}

