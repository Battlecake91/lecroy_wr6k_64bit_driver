
void __thiscall FUN_000192ac(void *this,ushort *param_1,int param_2,undefined4 param_3)

{
  ushort uVar1;
  undefined4 *puVar2;
  undefined4 uVar3;
  uint uVar4;
  undefined4 *puVar5;
  
  if ((DAT_0001d250 == (code *)0x0) && ((*(byte *)this & 1) != 0)) {
    FUN_0001896e();
  }
  if (((param_1 != (ushort *)0x0) && (uVar1 = *param_1, uVar1 != 0)) && (*(int *)(param_1 + 2) != 0)
     ) {
    *(uint *)((int)this + 0x10) = (uint)uVar1;
    puVar2 = (undefined4 *)FUN_0001039a(uVar1 + 4,0);
    *(undefined4 **)((int)this + 0xc) = puVar2;
    if (puVar2 == (undefined4 *)0x0) {
      *(undefined4 *)((int)this + 0x10) = 0;
    }
    else {
      uVar1 = *param_1;
      puVar5 = *(undefined4 **)(param_1 + 2);
      for (uVar4 = (uint)(uVar1 >> 2); uVar4 != 0; uVar4 = uVar4 - 1) {
        *puVar2 = *puVar5;
        puVar5 = puVar5 + 1;
        puVar2 = puVar2 + 1;
      }
      for (uVar4 = uVar1 & 3; uVar4 != 0; uVar4 = uVar4 - 1) {
        *(undefined1 *)puVar2 = *(undefined1 *)puVar5;
        puVar5 = (undefined4 *)((int)puVar5 + 1);
        puVar2 = (undefined4 *)((int)puVar2 + 1);
      }
      *(undefined1 *)(*(int *)((int)this + 0x10) + *(int *)((int)this + 0xc)) = 0x3a;
      *(int *)((int)this + 0x10) = *(int *)((int)this + 0x10) + 1;
      *(undefined1 *)(*(int *)((int)this + 0x10) + *(int *)((int)this + 0xc)) = 0x20;
      *(int *)((int)this + 0x10) = *(int *)((int)this + 0x10) + 1;
      *(undefined1 *)(*(int *)((int)this + 0x10) + *(int *)((int)this + 0xc)) = 0;
      *(undefined1 *)((int)this + 0x24) = 1;
    }
  }
  if (DAT_0001d250 != (code *)0x0) {
    if (param_2 == 0) {
      *(undefined4 *)((int)this + 0x14) = 0;
    }
    else {
      uVar3 = (*DAT_0001d250)(param_2,param_3,*(undefined4 *)this);
      *(undefined4 *)((int)this + 0x14) = uVar3;
    }
  }
  return;
}

