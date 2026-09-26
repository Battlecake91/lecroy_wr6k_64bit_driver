
undefined4 __thiscall FUN_00012f30(void *this,int param_1)

{
  ushort uVar1;
  undefined2 *puVar2;
  undefined2 *puVar3;
  undefined4 uVar4;
  byte bVar5;
  undefined2 *puVar6;
  byte bVar7;
  uint uVar8;
  undefined4 local_c;
  
  puVar6 = (undefined2 *)(param_1 + 8);
  local_c = 0xc0000001;
  uVar1 = *(ushort *)(param_1 + 2) >> 1;
  uVar8 = uVar1 & 0xff;
  bVar7 = (byte)uVar1;
  puVar2 = (undefined2 *)FUN_00010380(uVar8 << 2);
  if (puVar2 == (undefined2 *)0x0) {
    puVar2 = (undefined2 *)0x0;
  }
  else {
    _vector_constructor_iterator_(puVar2,4,uVar8,(_func_void_ptr_void_ptr *)&LAB_00011a7a);
  }
  if (puVar2 == (undefined2 *)0x0) {
    local_c = 0xc000009a;
  }
  else {
    FUN_000120dc((int)this);
    bVar5 = 0;
    puVar3 = puVar2;
    if (bVar7 != 0) {
      do {
        *(byte *)(puVar3 + 1) = bVar5;
        bVar5 = bVar5 + 1;
        *puVar3 = *puVar6;
        puVar6 = puVar6 + 1;
        puVar3 = puVar3 + 2;
      } while (bVar5 < bVar7);
    }
    uVar4 = FUN_00017bc8((void *)((int)this + 0x1086),(int)puVar2,bVar7,1);
    if ((char)uVar4 != '\0') {
      local_c = 0;
    }
    ExFreePool(puVar2);
  }
  return local_c;
}

