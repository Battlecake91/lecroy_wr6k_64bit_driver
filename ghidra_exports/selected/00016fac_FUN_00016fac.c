
undefined4 __thiscall FUN_00016fac(void *this,undefined4 *param_1)

{
  undefined4 *puVar1;
  byte bVar2;
  byte bVar3;
  undefined4 uVar4;
  int iVar5;
  uint uVar6;
  undefined8 uVar7;
  int local_c;
  char local_6;
  byte local_5;
  
  puVar1 = param_1;
  local_6 = '\n';
  if (param_1 == (undefined4 *)0x0) {
    uVar4 = 0xc000000d;
  }
  else {
    do {
      iVar5 = FUN_00016cb0(this);
      if (*(char *)((int)this + 0x18) == '\0') {
        return 0xc00000c0;
      }
      if (iVar5 == 0) {
        FUN_00016cf0(this,0x33);
        param_1 = (undefined4 *)0x0;
        *(undefined4 *)((int)this + 0x10) = 0;
        *(undefined4 *)((int)this + 0x14) = 0;
        do {
          bVar2 = FUN_00016d22(this);
          iVar5 = (int)(char)param_1._3_1_;
          bVar3 = param_1._3_1_ + 1;
          param_1 = (undefined4 *)((uint)bVar3 << 0x18);
          *(byte *)(iVar5 + 0x10 + (int)this) = bVar2;
        } while ((char)bVar3 < '\b');
      }
      param_1 = (undefined4 *)0x0;
      local_5 = 0;
      local_c = 0;
      *puVar1 = *(undefined4 *)((int)this + 0x10);
      iVar5 = *(int *)((int)this + 0x14);
      puVar1[1] = iVar5;
      do {
        uVar7 = __allshr((byte)local_c,iVar5);
        bVar3 = (byte)uVar7 & 1 ^ param_1._3_1_ >> 7;
        bVar3 = (bVar3 << 1 ^ bVar3) << 4 ^ (param_1._3_1_ << 1 | bVar3);
        param_1 = (undefined4 *)((uint)bVar3 << 0x18);
        if (local_c == 0x37) {
          bVar2 = 0;
          uVar6 = 7;
          do {
            local_5 = local_5 | (bVar3 >> ((byte)uVar6 & 0x1f) & 1) << (bVar2 & 0x1f);
            bVar2 = bVar2 + 1;
            uVar6 = uVar6 - 1;
          } while (uVar6 < 0x80000000);
        }
        local_c = local_c + 1;
      } while (local_c < 0x40);
      if ((local_5 == (byte)((uint)iVar5 >> 0x18)) && (bVar3 == 0)) {
        return 0;
      }
      local_6 = local_6 + -1;
      uVar4 = 0xc0000001;
    } while ('\0' < local_6);
  }
  return uVar4;
}

