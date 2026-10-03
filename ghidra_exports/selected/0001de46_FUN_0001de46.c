
undefined4 FUN_0001de46(int param_1,ushort *param_2)

{
  ushort uVar1;
  int *piVar2;
  undefined4 uVar3;
  undefined4 *puVar4;
  uint uVar5;
  uint uVar6;
  uint uVar7;
  undefined4 *puVar8;
  undefined4 *puVar9;
  ushort *unaff_retaddr;
  
  FUN_0001dd98();
  piVar2 = DAT_0001cdf8;
  if (DAT_0001cdf8 == (int *)0x0) {
    uVar3 = 0xc000009a;
  }
  else {
    DAT_0001cdf8[1] = param_1;
    puVar4 = (undefined4 *)FUN_0001039a((uint)(*param_2 >> 1) * 2 + 2,1);
    if (puVar4 == (undefined4 *)0x0) {
      puVar4 = (undefined4 *)0x0;
    }
    else {
      uVar1 = *param_2;
      puVar8 = *(undefined4 **)(param_2 + 2);
      puVar9 = puVar4;
      for (uVar5 = (uint)(uVar1 >> 2); uVar5 != 0; uVar5 = uVar5 - 1) {
        *puVar9 = *puVar8;
        puVar8 = puVar8 + 1;
        puVar9 = puVar9 + 1;
      }
      for (uVar5 = uVar1 & 3; uVar5 != 0; uVar5 = uVar5 - 1) {
        *(undefined1 *)puVar9 = *(undefined1 *)puVar8;
        puVar8 = (undefined4 *)((int)puVar8 + 1);
        puVar9 = (undefined4 *)((int)puVar9 + 1);
      }
      *(undefined2 *)((int)puVar4 + (uint)(*param_2 >> 1) * 2) = 0;
    }
    RtlInitUnicodeString(piVar2 + 2,puVar4);
    uVar5 = DAT_0001ce00;
    if ((short)DAT_0001ce00 != 0) {
      uVar7 = DAT_0001ce00 & 0xffff;
      uVar6 = DAT_0001ce00 & 0xfffe;
      puVar4 = (undefined4 *)FUN_0001039a(uVar6 + 2,1);
      param_2 = unaff_retaddr;
      if (puVar4 != (undefined4 *)0x0) {
        puVar8 = DAT_0001ce04;
        puVar9 = puVar4;
        for (uVar7 = uVar7 >> 2; uVar7 != 0; uVar7 = uVar7 - 1) {
          *puVar9 = *puVar8;
          puVar8 = puVar8 + 1;
          puVar9 = puVar9 + 1;
        }
        for (uVar5 = uVar5 & 3; uVar5 != 0; uVar5 = uVar5 - 1) {
          *(undefined1 *)puVar9 = *(undefined1 *)puVar8;
          puVar8 = (undefined4 *)((int)puVar8 + 1);
          puVar9 = (undefined4 *)((int)puVar9 + 1);
        }
        *(undefined2 *)((int)puVar4 + uVar6) = 0;
        RtlInitUnicodeString(&DAT_0001ce00,puVar4);
      }
    }
    FUN_0001ddc6((int)piVar2);
    uVar3 = (**(code **)(*piVar2 + 4))(param_2);
    *(bool *)(piVar2 + 4) = *(int *)(piVar2[1] + 4) != 0;
  }
  return uVar3;
}

