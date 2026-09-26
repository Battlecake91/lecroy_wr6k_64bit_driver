
undefined2 __thiscall FUN_00015c7e(void *this,byte *param_1)

{
  undefined4 *puVar1;
  byte bVar2;
  short sVar3;
  ushort uVar4;
  byte *pbVar5;
  short sVar6;
  undefined4 *puVar7;
  undefined2 uVar8;
  byte *pbVar9;
  uint local_18;
  short local_c;
  undefined4 *local_8;
  
  pbVar5 = param_1;
  sVar3 = *(short *)(param_1 + 3);
  puVar7 = (undefined4 *)FUN_00010380((uint)(ushort)(sVar3 + 8U));
  local_c = 0;
  sVar6 = local_c;
  local_c = 0;
  bVar2 = *param_1;
  if (bVar2 < 2) {
    uVar4 = *(ushort *)(param_1 + 5);
    pbVar9 = param_1 + 9;
    local_8 = puVar7 + 2;
    param_1._0_2_ = uVar4;
    local_c = sVar6;
    if (0xf < uVar4) {
      FUN_00015802(*(void **)((int)this + 0x15),0x10,bVar2);
      local_18 = (uint)(uVar4 >> 4);
      local_c = (uVar4 >> 4) * 2;
      param_1 = (byte *)(uint)uVar4;
      do {
        FUN_00015848(*(void **)((int)this + 0x15),*(undefined2 *)pbVar9,*(undefined2 *)(pbVar9 + 2))
        ;
        pbVar9 = pbVar9 + 4;
        FUN_0001586e(*(char **)((int)this + 0x15));
        puVar1 = (undefined4 *)((int)local_8 + 2);
        param_1 = param_1 + 0xfff0;
        local_18 = local_18 - 1;
        *(undefined2 *)local_8 = 0;
        local_8 = puVar1;
      } while (local_18 != 0);
    }
    if ((ushort)param_1 != 0) {
      FUN_00015802(*(void **)((int)this + 0x15),(byte)(ushort)param_1,*pbVar5);
      FUN_00015848(*(void **)((int)this + 0x15),*(undefined2 *)pbVar9,*(undefined2 *)(pbVar9 + 2));
      FUN_0001586e(*(char **)((int)this + 0x15));
      local_c = local_c + 2;
      *(short *)local_8 = (short)(0 >> (0x20 - (byte)(ushort)param_1 & 0x1f));
    }
    uVar8 = 0;
  }
  else {
    uVar8 = 4;
  }
  *puVar7 = 0;
  *(short *)(puVar7 + 1) = local_c + 2;
  *(undefined2 *)((int)puVar7 + 6) = uVar8;
  FUN_00015a26(this,puVar7,sVar3 + 8U);
  ExFreePool(puVar7);
  return uVar8;
}

