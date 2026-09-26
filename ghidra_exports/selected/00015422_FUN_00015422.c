
uint __thiscall FUN_00015422(void *this,int param_1)

{
  int iVar1;
  uint uVar2;
  
  iVar1 = *(int *)((int)this + 8);
  if (param_1 == 0) {
    uVar2 = 0;
    if (iVar1 != 0) {
      uVar2 = ExFreePool(iVar1);
      *(undefined4 *)((int)this + 8) = 0;
      *(undefined4 *)((int)this + 0x10) = 0;
      *(undefined4 *)((int)this + 0xc) = 0;
    }
LAB_0001547c:
    uVar2 = uVar2 & 0xffffff00;
  }
  else {
    if (*(int *)((int)this + 0xc) != param_1) {
      if (iVar1 != 0) {
        ExFreePool(iVar1);
        *(undefined4 *)((int)this + 8) = 0;
        *(undefined4 *)((int)this + 0x10) = 0;
        *(undefined4 *)((int)this + 0xc) = 0;
      }
      iVar1 = FUN_00010380(param_1 + 4);
      *(int *)((int)this + 8) = iVar1;
      uVar2 = 0;
      if (iVar1 == 0) goto LAB_0001547c;
    }
    *(int *)((int)this + 0x10) = iVar1;
    *(int *)((int)this + 0xc) = param_1;
    uVar2 = CONCAT31((int3)((uint)iVar1 >> 8),1);
  }
  return uVar2;
}

