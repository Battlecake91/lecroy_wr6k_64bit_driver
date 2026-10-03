
uint __fastcall FUN_000174c8(int param_1)

{
  uint uVar1;
  uint uVar2;
  
  uVar1 = READ_REGISTER_ULONG(*(undefined4 *)(param_1 + 0xd8));
  uVar2 = uVar1 >> 5;
  if ((uVar2 & 1) != 0) {
    uVar1 = FUN_000107fe((undefined4 *)(param_1 + 0xd8),uVar1 & 0xffffffdf);
  }
  return CONCAT31((int3)(uVar1 >> 8),(char)uVar2) & 0xffffff01;
}

