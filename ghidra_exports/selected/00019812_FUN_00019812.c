
undefined4 __fastcall FUN_00019812(int param_1)

{
  undefined4 *puVar1;
  int iVar2;
  int iVar3;
  undefined4 *puVar4;
  undefined4 unaff_EDI;
  undefined4 *puVar5;
  
  puVar1 = (undefined4 *)ExAllocatePoolWithTag(1,0x40,0x206d6457);
  if (puVar1 == (undefined4 *)0x0) {
    unaff_EDI = 0xc000009a;
  }
  else {
    puVar4 = puVar1;
    for (iVar3 = 0x10; iVar3 != 0; iVar3 = iVar3 + -1) {
      *puVar4 = 0;
      puVar4 = puVar4 + 1;
    }
    puVar1[3] = 0xffffffff;
    puVar1[2] = 0xffffffff;
    *(undefined2 *)puVar1 = 0x40;
    *(undefined2 *)((int)puVar1 + 2) = 1;
    iVar3 = IoAllocateIrp(*(undefined1 *)(*(int *)(*(int *)(param_1 + 0x30) + 4) + 0x30),0);
    if (iVar3 == 0) {
      unaff_EDI = 0xc000009a;
    }
    else {
      *(undefined1 *)(*(int *)(iVar3 + 0x60) + -0x24) = 0x1b;
      *(undefined1 *)(*(int *)(iVar3 + 0x60) + -0x23) = 9;
      *(undefined4 **)(*(int *)(iVar3 + 0x60) + -0x20) = puVar1;
      *(undefined4 *)(iVar3 + 0x18) = 0xc00000bb;
      iVar2 = FUN_0001bed2(iVar3,1,(undefined4 *)0x0);
      if (-1 < iVar2) {
        puVar4 = puVar1;
        puVar5 = (undefined4 *)(param_1 + 0x164);
        for (iVar2 = 0x10; iVar2 != 0; iVar2 = iVar2 + -1) {
          *puVar5 = *puVar4;
          puVar4 = puVar4 + 1;
          puVar5 = puVar5 + 1;
        }
      }
      ExFreePool(puVar1);
      IoFreeIrp(iVar3);
    }
  }
  return unaff_EDI;
}

