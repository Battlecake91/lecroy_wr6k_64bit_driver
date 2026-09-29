
undefined4 __thiscall FUN_00013230(void *this,uint param_1,undefined4 *param_2)

{
  uint uVar1;
  int iVar2;
  undefined4 *puVar3;
  
  uVar1 = FUN_000121fa(this,param_1);
  if (-1 < (int)uVar1) {
    puVar3 = (undefined4 *)(param_1 * 0x10a + *(int *)((int)this + 0x10));
    for (iVar2 = 0x42; iVar2 != 0; iVar2 = iVar2 + -1) {
      *puVar3 = *param_2;
      param_2 = param_2 + 1;
      puVar3 = puVar3 + 1;
    }
    *(undefined2 *)puVar3 = *(undefined2 *)param_2;
    if (*(int *)((int)this + 0xc) < (int)param_1) {
      *(uint *)((int)this + 0xc) = param_1;
    }
  }
  return *(undefined4 *)((int)this + 0x14);
}

