
uint __thiscall FUN_00015e80(void *this,ushort *param_1)

{
  ushort *puVar1;
  uint *puVar2;
  ushort uVar3;
  int iVar4;
  ushort *puVar5;
  uint uVar6;
  uint uVar7;
  uint local_8;
  
  puVar5 = param_1;
  if (*(short *)((int)param_1 + 3) != 0) {
    FUN_000158c0(*(void **)((int)this + 0x19),(byte)*param_1);
  }
  uVar3 = *(ushort *)((int)param_1 + 1);
  uVar7 = (uint)uVar3;
  puVar1 = (ushort *)((int)param_1 + 5);
  uVar6 = FUN_000155d0(*(void **)((int)this + 0x19),(byte)*param_1,0);
  if ((char)uVar6 == '\0') {
    uVar6 = CONCAT22((short)(uVar6 >> 0x10),4);
  }
  else {
    iVar4 = *(int *)((int)this + 0x19);
    if (uVar3 < 0x11) {
      *(uint *)(iVar4 + 0x10) = *(uint *)(iVar4 + 0x10) ^ (uVar7 ^ *(uint *)(iVar4 + 0x10)) & 0x1f;
    }
    else {
      *(uint *)(iVar4 + 0x10) = *(uint *)(iVar4 + 0x10) & 0xfffffff0 | 0x10;
    }
    FUN_0001236e(*(char **)((int)this + 0x19));
    param_1 = puVar1;
    if (0xf < uVar3) {
      local_8 = (uint)(uVar3 >> 4);
      do {
        uVar3 = *param_1;
        param_1 = param_1 + 1;
        FUN_0001588e(*(void **)((int)this + 0x19),(uint)uVar3);
        uVar7 = uVar7 + 0xfff0;
        local_8 = local_8 - 1;
      } while (local_8 != 0);
    }
    if ((short)uVar7 != 0) {
      uVar3 = *param_1;
      puVar2 = (uint *)(*(int *)((int)this + 0x19) + 0x10);
      *puVar2 = *puVar2 ^ (uVar7 ^ *(uint *)(*(int *)((int)this + 0x19) + 0x10)) & 0x1f;
      FUN_0001236e(*(char **)((int)this + 0x19));
      FUN_0001588e(*(void **)((int)this + 0x19),(uint)uVar3);
    }
    FUN_000155d0(*(void **)((int)this + 0x19),(byte)*puVar5,1);
    puVar2 = (uint *)(*(int *)((int)this + 0x19) + 0x10);
    *puVar2 = *puVar2 & 0xffffffe0;
    FUN_0001236e(*(char **)((int)this + 0x19));
    uVar6 = FUN_00015a88(this,0);
    uVar6 = uVar6 & 0xffff0000;
  }
  return uVar6;
}

