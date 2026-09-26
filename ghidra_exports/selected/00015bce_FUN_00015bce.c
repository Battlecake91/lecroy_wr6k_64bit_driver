
undefined2 __fastcall FUN_00015bce(void *param_1)

{
  undefined4 *puVar1;
  undefined4 uVar2;
  undefined2 uVar3;
  
  uVar2 = 0;
  puVar1 = (undefined4 *)FUN_00010380(0xc);
  if (*(undefined4 **)((int)param_1 + 0x17e) == (undefined4 *)0x0) {
    uVar3 = 8;
  }
  else {
    uVar2 = READ_REGISTER_ULONG(**(undefined4 **)((int)param_1 + 0x17e));
    uVar3 = 0;
  }
  *puVar1 = 0;
  *(undefined2 *)((int)puVar1 + 6) = uVar3;
  *(undefined2 *)(puVar1 + 1) = 6;
  puVar1[2] = uVar2;
  FUN_00015a26(param_1,puVar1,0xc);
  ExFreePool(puVar1);
  return uVar3;
}

