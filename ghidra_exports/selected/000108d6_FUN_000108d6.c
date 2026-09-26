
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

uint FUN_000108d6(int param_1)

{
  uint uVar1;
  uint uVar2;
  undefined4 uVar3;
  uint local_c;
  
  local_c = 0;
  uVar1 = READ_REGISTER_ULONG(*(undefined4 *)(param_1 + 0x390));
  if ((DAT_0001ce44 & uVar1) == 0) {
    uVar1 = uVar1 & 0xffffff00;
  }
  else {
    _DAT_0001ce10 = _DAT_0001ce10 | uVar1 & DAT_0001ce14;
    if ((uVar1 & 1) != 0) {
      *(undefined4 *)(param_1 + 0x3dc) = 0;
      WRITE_REGISTER_ULONG(*(undefined4 *)(param_1 + 0x3b8),0);
    }
    if ((uVar1 >> 1 & 1) != 0) {
      local_c = READ_REGISTER_ULONG(*(undefined4 *)(param_1 + 0x340));
      *(uint *)(param_1 + 0x134a) = *(uint *)(param_1 + 0x134a) | local_c;
      uVar2 = (uint)((local_c & 0x400) != 0);
      if ((local_c & 0x800) != 0) {
        uVar2 = uVar2 | 2;
      }
      if ((local_c & 0x1000) != 0) {
        uVar2 = uVar2 | 4;
      }
      if ((local_c & 0x2000) != 0) {
        uVar2 = uVar2 | 8;
      }
      if ((local_c & 0x4000) != 0) {
        uVar2 = uVar2 | 0x10;
      }
      if (uVar2 != 0) {
        FUN_000107fe((void *)(param_1 + 0x408),uVar2);
      }
      FUN_000107fe((void *)(param_1 + 0x340),local_c);
    }
    if ((uVar1 & 4) != 0) {
      *(undefined4 *)(param_1 + 0x404) = 1;
      WRITE_REGISTER_ULONG(*(undefined4 *)(param_1 + 0x3e0),1);
    }
    if ((uVar1 & 8) != 0) {
      *(undefined4 *)(param_1 + 0x404) = 2;
      WRITE_REGISTER_ULONG(*(undefined4 *)(param_1 + 0x3e0),2);
    }
    if ((uVar1 & 0x10) != 0) {
      *(undefined4 *)(param_1 + 0x404) = 4;
      WRITE_REGISTER_ULONG(*(undefined4 *)(param_1 + 0x3e0),4);
    }
    if ((uVar1 & 0x20) != 0) {
      *(undefined4 *)(param_1 + 0x404) = 8;
      WRITE_REGISTER_ULONG(*(undefined4 *)(param_1 + 0x3e0),8);
    }
    FUN_000107fe((undefined4 *)(param_1 + 0x390),uVar1);
    if ((uVar1 >> 1 & 1) != 0) {
      KeStallExecutionProcessor(1);
      uVar1 = READ_REGISTER_ULONG(*(undefined4 *)(param_1 + 0x390));
      if (((uVar1 & 2) != 0) &&
         (uVar1 = READ_REGISTER_ULONG(*(undefined4 *)(param_1 + 0x340)), uVar1 == local_c)) {
        FUN_000107fe((void *)(param_1 + 0x368),local_c | *(uint *)(param_1 + 0x38c));
        *(byte *)(param_1 + 0x134d) = *(byte *)(param_1 + 0x134d) | 0x80;
      }
    }
    uVar3 = KeInsertQueueDpc(param_1 + 0x1515,0,0);
    uVar1 = CONCAT31((int3)((uint)uVar3 >> 8),1);
  }
  return uVar1;
}

