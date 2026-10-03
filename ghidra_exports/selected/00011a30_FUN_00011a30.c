
void __fastcall FUN_00011a30(undefined4 *param_1)

{
  uint uVar1;
  uint *puVar2;
  int local_4;
  
  uVar1 = 0;
  puVar2 = param_1 + 10;
  local_4 = 0x100;
  do {
    if ((*puVar2 & 0xffff) != 0xffff) {
      uVar1 = (uint)*(byte *)((int)puVar2 + 2) << 0x10 | *puVar2 & 0xffff;
      WRITE_REGISTER_ULONG(*param_1,uVar1);
    }
    puVar2 = puVar2 + 1;
    local_4 = local_4 + -1;
  } while (local_4 != 0);
  WRITE_REGISTER_ULONG(*param_1,uVar1);
  return;
}

