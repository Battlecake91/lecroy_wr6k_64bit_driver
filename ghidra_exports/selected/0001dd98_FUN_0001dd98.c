
void FUN_0001dd98(void)

{
  undefined4 *puVar1;
  undefined4 *extraout_ECX;
  
  puVar1 = (undefined4 *)ExAllocatePoolWithTag(0,0x20,0x206d6457);
  if (puVar1 != (undefined4 *)0x0) {
    FUN_0001dd80(puVar1);
    *extraout_ECX = &PTR_FUN_0001c4d8;
  }
  RtlInitUnicodeString(&DAT_0001ce00,0);
  return;
}

