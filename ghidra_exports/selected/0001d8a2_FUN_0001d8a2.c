
undefined4 __thiscall
FUN_0001d8a2(void *this,ushort *param_1,ushort *param_2,undefined4 param_3,undefined4 param_4,
            char param_5,undefined4 param_6)

{
  bool bVar1;
  undefined4 uVar2;
  void *pvVar3;
  ushort *puVar4;
  undefined4 local_28;
  undefined4 local_24;
  ushort *local_20;
  undefined4 local_1c;
  undefined4 local_18;
  undefined4 local_14;
  ushort local_10;
  ushort local_e;
  void *local_c;
  void *local_8;
  
  local_8 = this;
  if (*(int *)((int)this + 4) != 0) {
    FUN_000103c8((int)this);
  }
  puVar4 = param_2;
  if (param_2 != (ushort *)0x0) {
    if (((*param_2 == 0) || (param_2[1] == 0)) || (*(int *)(param_2 + 2) == 0)) {
      param_2 = (ushort *)0x0;
    }
    puVar4 = param_2;
    if (param_2 != (ushort *)0x0) {
      if ((*(short *)(*(int *)(param_1 + 2) + -2 + (uint)(*param_1 >> 1) * 2) == 0x5c) ||
         (**(short **)(param_2 + 2) == 0x5c)) {
        bVar1 = false;
        param_2 = (ushort *)(uint)*param_2;
      }
      else {
        bVar1 = true;
        param_2 = (ushort *)(uint)*param_2;
      }
      goto LAB_0001d907;
    }
  }
  bVar1 = false;
  param_2 = (ushort *)0x0;
LAB_0001d907:
  local_10 = *param_1 + (short)param_2;
  if (bVar1) {
    local_10 = local_10 + 2;
  }
  local_e = local_10;
  local_c = (void *)FUN_0001039a((uint)(local_10 >> 1) << 1,1);
  if (local_c == (void *)0x0) {
    uVar2 = 0xc000009a;
  }
  else {
    memmove(local_c,*(void **)(param_1 + 2),(uint)*param_1);
    if (puVar4 != (ushort *)0x0) {
      if (bVar1) {
        *(undefined2 *)((int)local_c + (uint)(*param_1 >> 1) * 2) = 0x5c;
      }
      memmove((void *)((int)local_c + ((uint)(*param_1 >> 1) + (uint)bVar1) * 2),
              *(void **)(puVar4 + 2),(size_t)param_2);
    }
    pvVar3 = local_8;
    local_1c = param_4;
    local_20 = &local_10;
    local_28 = 0x18;
    local_24 = 0;
    local_18 = 0;
    local_14 = 0;
    if (param_5 == '\0') {
      uVar2 = ZwOpenKey((int)local_8 + 4,param_3,&local_28);
      *(undefined4 *)((int)local_8 + 8) = uVar2;
      pvVar3 = local_8;
    }
    else {
      uVar2 = ZwCreateKey((int)local_8 + 4,param_3,&local_28,0,0,param_6,local_8);
      *(undefined4 *)((int)pvVar3 + 8) = uVar2;
    }
    if (local_c != (void *)0x0) {
      ExFreePool(local_c);
    }
    *(undefined4 *)((int)pvVar3 + 0xc) = 0;
    uVar2 = *(undefined4 *)((int)pvVar3 + 8);
  }
  return uVar2;
}

