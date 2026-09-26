
uint __thiscall FUN_000121fa(void *this,uint param_1)

{
  uint uVar1;
  int iVar2;
  int iVar3;
  int iVar4;
  uint uVar5;
  int iVar6;
  undefined4 *puVar7;
  undefined4 *puVar8;
  
  *(undefined4 *)((int)this + 0x14) = 0;
  uVar5 = *(uint *)this;
  if (uVar5 <= param_1) {
    uVar1 = *(uint *)((int)this + 4);
    iVar6 = (((uVar1 - uVar5) + param_1) / uVar1) * uVar1 + uVar5;
    iVar2 = FUN_0001039a(iVar6 * 0x10a,*(undefined4 *)((int)this + 8));
    uVar5 = 0;
    if (iVar2 == 0) {
      *(undefined4 *)((int)this + 0x14) = 0xc000009a;
    }
    else {
      if (*(int *)this != 0) {
        iVar3 = 0;
        do {
          puVar7 = (undefined4 *)(*(int *)((int)this + 0x10) + iVar3);
          puVar8 = (undefined4 *)(iVar3 + iVar2);
          for (iVar4 = 0x42; iVar4 != 0; iVar4 = iVar4 + -1) {
            *puVar8 = *puVar7;
            puVar7 = puVar7 + 1;
            puVar8 = puVar8 + 1;
          }
          uVar5 = uVar5 + 1;
          *(undefined2 *)puVar8 = *(undefined2 *)puVar7;
          iVar3 = iVar3 + 0x10a;
        } while (uVar5 < *(uint *)this);
      }
      *(int *)this = iVar6;
      if (*(int *)((int)this + 0x10) != 0) {
        ExFreePool(*(int *)((int)this + 0x10));
      }
      *(int *)((int)this + 0x10) = iVar2;
    }
  }
  return *(uint *)((int)this + 0x14);
}

