
void __thiscall FUN_00015a88(void *this,undefined2 param_1)

{
  undefined4 *puVar1;
  
  puVar1 = (undefined4 *)FUN_00010380(8);
  if (puVar1 != (undefined4 *)0x0) {
    *puVar1 = 0xffffffff;
    puVar1[1] = 0xffffffff;
    *puVar1 = 0;
    *(undefined2 *)(puVar1 + 1) = 2;
    *(undefined2 *)((int)puVar1 + 6) = param_1;
    FUN_00015a26(this,puVar1,8);
    ExFreePool(puVar1);
  }
  return;
}

