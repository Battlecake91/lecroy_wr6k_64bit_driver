
undefined2 __thiscall FUN_00015b64(void *this,char *param_1)

{
  undefined4 *puVar1;
  undefined4 *puVar2;
  undefined2 uVar3;
  undefined4 uVar4;
  
  uVar4 = 0;
  puVar1 = (undefined4 *)FUN_00010380(0xc);
  if (*param_1 == '\0') {
    puVar2 = *(undefined4 **)((int)this + 0x16a);
  }
  else {
    if (*param_1 != '\x01') {
      uVar3 = 4;
      goto LAB_00015ba3;
    }
    puVar2 = *(undefined4 **)((int)this + 0x172);
  }
  uVar4 = READ_REGISTER_ULONG(*puVar2);
  uVar3 = 0;
LAB_00015ba3:
  *puVar1 = 0;
  *(undefined2 *)((int)puVar1 + 6) = uVar3;
  *(undefined2 *)(puVar1 + 1) = 6;
  puVar1[2] = uVar4;
  FUN_00015a26(this,puVar1,0xc);
  ExFreePool(puVar1);
  return uVar3;
}

