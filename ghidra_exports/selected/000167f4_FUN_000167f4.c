
uint __thiscall FUN_000167f4(void *this,int *param_1,ushort *param_2)

{
  int *piVar1;
  ushort uVar2;
  undefined4 *puVar3;
  int iVar4;
  undefined3 uVar6;
  undefined4 uVar5;
  uint uVar7;
  uint local_c;
  undefined1 local_8;
  undefined1 local_7;
  undefined1 local_6;
  undefined1 local_5;
  
  if (*(char *)((int)this + 0x24) == '\0') {
    if (*(char *)((int)this + 0x23) == '\0') {
      if ((*(short *)((int)this + 0x21) != 0) && (*(int *)((int)this + 0x1d) != 0)) {
        ExFreePool(*(int *)((int)this + 0x1d));
        *(undefined4 *)((int)this + 0x1d) = 0;
        *(undefined2 *)((int)this + 0x21) = 0;
      }
      puVar3 = (undefined4 *)FUN_00010380(8);
      piVar1 = (int *)((int)this + 0x1d);
      *piVar1 = (int)puVar3;
      if (puVar3 == (undefined4 *)0x0) {
        *param_1 = 0;
        *param_2 = 0;
        return (uint)param_2 & 0xffffff00;
      }
      *(undefined2 *)((int)this + 0x21) = 8;
      *puVar3 = 0;
      *(undefined2 *)(*piVar1 + 4) = 2;
      *(undefined2 *)(*piVar1 + 6) = 0x20;
      iVar4 = *piVar1;
      *param_1 = iVar4;
      uVar5 = CONCAT22((short)((uint)iVar4 >> 0x10),*(ushort *)((int)this + 0x21));
      *param_2 = *(ushort *)((int)this + 0x21);
      *(undefined1 *)((int)this + 0x24) = 0;
    }
    else {
      iVar4 = *(int *)((int)this + 0x1d);
      *param_1 = iVar4;
      uVar5 = CONCAT22((short)((uint)iVar4 >> 0x10),*(ushort *)((int)this + 0x21));
      *param_2 = *(ushort *)((int)this + 0x21);
    }
    *(undefined1 *)((int)this + 0x23) = 0;
    uVar6 = (undefined3)((uint)uVar5 >> 8);
  }
  else {
    uVar2 = *param_2;
    if ((*(short *)((int)this + 0x21) != 0) && (*(int *)((int)this + 0x1d) != 0)) {
      ExFreePool(*(int *)((int)this + 0x1d));
      *(undefined4 *)((int)this + 0x1d) = 0;
      *(undefined2 *)((int)this + 0x21) = 0;
    }
    local_c = (uint)uVar2;
    puVar3 = (undefined4 *)FUN_00010380(local_c);
    *(undefined4 **)((int)this + 0x1d) = puVar3;
    if (puVar3 != (undefined4 *)0x0) {
      *(ushort *)((int)this + 0x21) = uVar2;
      for (uVar7 = local_c >> 2; uVar7 != 0; uVar7 = uVar7 - 1) {
        *puVar3 = 0xffffffff;
        puVar3 = puVar3 + 1;
      }
      for (local_c = local_c & 3; local_c != 0; local_c = local_c - 1) {
        *(undefined1 *)puVar3 = 0xff;
        puVar3 = (undefined4 *)((int)puVar3 + 1);
      }
      local_8 = 0xfb;
      local_7 = 0x85;
      local_6 = 0x40;
      local_5 = 0;
      local_c = 0;
      iVar4 = FUN_0001619a(this,(ushort *)&local_8,4,(undefined2 *)(*(int *)((int)this + 0x1d) + 6),
                           uVar2 - 6 & 0xffff,(int *)&local_c);
      **(int **)((int)this + 0x1d) = iVar4;
      if (iVar4 == 0) {
        *(undefined2 *)(*(int *)((int)this + 0x1d) + 4) = (undefined2)local_c;
      }
      else {
        *(undefined2 *)(*(int *)((int)this + 0x1d) + 6) = 0x20;
        *(undefined2 *)(*(int *)((int)this + 0x1d) + 4) = 2;
      }
    }
    iVar4 = *(int *)((int)this + 0x1d);
    *param_1 = iVar4;
    uVar2 = *(ushort *)((int)this + 0x21);
    *param_2 = uVar2;
    *(undefined1 *)((int)this + 0x23) = 0;
    *(undefined1 *)((int)this + 0x24) = 0;
    uVar6 = (undefined3)(CONCAT22((short)((uint)iVar4 >> 0x10),uVar2) >> 8);
  }
  return CONCAT31(uVar6,1);
}

