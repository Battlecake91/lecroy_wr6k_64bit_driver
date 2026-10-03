
bool __cdecl FUN_0001859c(undefined *param_1)

{
  undefined4 *puVar1;
  
  puVar1 = (undefined4 *)FUN_00010380(8);
  if (puVar1 == (undefined4 *)0x0) {
    puVar1 = (undefined4 *)0x0;
  }
  else {
    puVar1[1] = param_1;
    *puVar1 = DAT_0001ce48;
    DAT_0001ce48 = puVar1;
  }
  if (puVar1 == (undefined4 *)0x0) {
    (*(code *)param_1)();
    DAT_0001ce4c = 0xc000009a;
  }
  return puVar1 != (undefined4 *)0x0;
}

