
undefined4 FUN_0001c118(int param_1,PVOID param_2,undefined4 param_3)

{
  int iVar1;
  undefined1 *puVar2;
  int iVar3;
  undefined1 *puVar4;
  int iStack_c;
  undefined4 uStack_8;
  
  puVar4 = &stack0xfffffffc;
  puVar2 = &stack0xfffffffc;
  if ((*(uint *)(param_1 + 4) & 6) == 0) {
    iStack_c = param_1;
    uStack_8 = param_3;
    *(int **)((int)param_2 + -4) = &iStack_c;
    iVar1 = *(int *)((int)param_2 + 8);
    for (iVar3 = *(int *)((int)param_2 + 0xc); iVar3 != -1; iVar3 = *(int *)(iVar1 + iVar3 * 0xc)) {
      if (*(int *)(iVar1 + 4 + iVar3 * 0xc) != 0) {
        iVar1 = (**(code **)(iVar1 + 4 + iVar3 * 0xc))(puVar2,iVar3,puVar4);
        param_2 = *(PVOID *)(puVar2 + 0xc);
        if (iVar1 != 0) {
          if (iVar1 < 0) {
            return 0;
          }
          iVar1 = *(int *)((int)param_2 + 8);
          __global_unwind2(param_2);
          puVar2 = (undefined1 *)((int)param_2 + 0x10);
          FUN_0001c226((int)param_2,iVar3);
          *(undefined4 *)((int)param_2 + 0xc) = *(undefined4 *)(iVar1 + iVar3 * 0xc);
          (**(code **)(iVar1 + 8 + iVar3 * 0xc))();
        }
      }
      iVar1 = *(int *)((int)param_2 + 8);
    }
  }
  else {
    FUN_0001c226((int)param_2,-1);
  }
  return 1;
}

