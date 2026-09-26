
undefined4 __thiscall FUN_000153a2(void *this,uint param_1,undefined4 param_2)

{
  ushort uVar1;
  undefined4 uVar2;
  void *extraout_ECX;
  int iVar3;
  
  *(undefined4 *)((int)this + 0xc) = 0;
  *(undefined4 *)((int)this + 0x10) = 0;
  *(undefined4 *)((int)this + 8) = 0;
  *(undefined4 *)((int)this + 0x18) = 0;
  *(undefined4 *)((int)this + 0x14) = 0;
  *(undefined4 *)((int)this + 0x1c) = 0;
  if (param_1 == 0) {
    uVar2 = 0;
  }
  else {
    *(uint *)((int)this + 0xc) = param_1;
    *(uint *)((int)this + 0x10) = param_1;
    *(undefined4 *)((int)this + 8) = param_2;
    iVar3 = 0;
    do {
      uVar1 = **(ushort **)((int)this + 0x10);
      *(int *)((int)this + 0x18) = *(int *)((int)this + 0x18) + *(ushort *)(param_1 + 2) + 8;
      iVar3 = iVar3 + (uint)uVar1;
      *(int *)((int)this + 0x1c) = *(int *)((int)this + 0x1c) + 1;
      param_1 = FUN_00015364((int)this);
      this = extraout_ECX;
    } while (param_1 != 0);
    *(int *)((int)extraout_ECX + 0x14) = iVar3;
    *(undefined4 *)((int)extraout_ECX + 0x10) = *(undefined4 *)((int)extraout_ECX + 0xc);
    uVar2 = CONCAT31((int3)((uint)*(undefined4 *)((int)extraout_ECX + 0xc) >> 8),1);
  }
  return uVar2;
}

