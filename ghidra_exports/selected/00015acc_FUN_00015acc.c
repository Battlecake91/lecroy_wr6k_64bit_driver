
uint __thiscall FUN_00015acc(void *this,byte *param_1)

{
  byte *pbVar1;
  uint uVar2;
  byte local_8;
  
  if (*param_1 < 2) {
    local_8 = param_1[1];
    pbVar1 = param_1 + 5;
    if (0xf < local_8) {
      FUN_00015802(*(void **)((int)this + 0x15),0x10,*param_1);
      uVar2 = (uint)(local_8 >> 4);
      do {
        FUN_00015848(*(void **)((int)this + 0x15),*(undefined2 *)pbVar1,*(undefined2 *)(pbVar1 + 2))
        ;
        local_8 = local_8 - 0x10;
        pbVar1 = pbVar1 + 4;
        uVar2 = uVar2 - 1;
      } while (uVar2 != 0);
    }
    if (local_8 != 0) {
      FUN_00015802(*(void **)((int)this + 0x15),local_8,*param_1);
      FUN_00015848(*(void **)((int)this + 0x15),*(undefined2 *)pbVar1,*(undefined2 *)(pbVar1 + 2));
    }
    uVar2 = FUN_00015a88(this,0);
    uVar2 = uVar2 & 0xffff0000;
  }
  else {
    uVar2 = CONCAT22((short)((uint)param_1 >> 0x10),4);
  }
  return uVar2;
}

