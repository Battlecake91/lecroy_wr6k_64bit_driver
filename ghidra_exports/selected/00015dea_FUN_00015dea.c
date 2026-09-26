
undefined2 __thiscall FUN_00015dea(void *this,char *param_1)

{
  ushort *puVar1;
  char cVar2;
  void **ppvVar3;
  int iVar4;
  undefined4 *puVar5;
  undefined4 uVar6;
  void *local_8;
  
  local_8 = this;
  puVar1 = (ushort *)(param_1 + 1);
  cVar2 = *param_1;
  ppvVar3 = (void **)0x0;
  param_1._0_2_ = 0x10;
  if (cVar2 == '\0') {
    iVar4 = *(int *)((int)this + 0x25);
  }
  else if (cVar2 == '\x01') {
    iVar4 = *(int *)((int)this + 0x29);
  }
  else {
    if (cVar2 != '\x02') {
      param_1._0_2_ = 4;
      goto LAB_00015e32;
    }
    iVar4 = *(int *)((int)this + 0x2d);
  }
  ppvVar3 = &local_8;
  local_8 = (void *)((uint)*puVar1 + *(int *)(iVar4 + 0x10));
LAB_00015e32:
  uVar6 = 0;
  if (ppvVar3 != (void **)0x0) {
    uVar6 = READ_REGISTER_ULONG(*ppvVar3);
    param_1._0_2_ = 0;
  }
  puVar5 = (undefined4 *)FUN_00010380(0xc);
  *puVar5 = 0;
  *(undefined2 *)((int)puVar5 + 6) = param_1._0_2_;
  *(undefined2 *)(puVar5 + 1) = 6;
  puVar5[2] = uVar6;
  FUN_00015a26(this,puVar5,0xc);
  ExFreePool(puVar5);
  return param_1._0_2_;
}

