
uint __thiscall FUN_00017b00(void *this,uint param_1)

{
  uint uVar1;
  int iVar2;
  uint uVar3;
  int iVar4;
  
  uVar3 = *(uint *)this;
  *(undefined4 *)((int)this + 0x14) = 0;
  if (uVar3 <= param_1) {
    uVar1 = *(uint *)((int)this + 4);
    iVar4 = (((uVar1 - uVar3) + param_1) / uVar1) * uVar1 + uVar3;
    iVar2 = FUN_0001039a(iVar4,*(undefined4 *)((int)this + 8));
    if (iVar2 == 0) {
      *(undefined4 *)((int)this + 0x14) = 0xc000009a;
    }
    else {
      uVar3 = 0;
      if (*(int *)this != 0) {
        do {
          *(undefined1 *)(uVar3 + iVar2) = *(undefined1 *)(uVar3 + *(int *)((int)this + 0x10));
          uVar3 = uVar3 + 1;
        } while (uVar3 < *(uint *)this);
      }
      *(int *)this = iVar4;
      if (*(int *)((int)this + 0x10) != 0) {
        ExFreePool(*(int *)((int)this + 0x10));
      }
      *(int *)((int)this + 0x10) = iVar2;
    }
  }
  return *(uint *)((int)this + 0x14);
}

