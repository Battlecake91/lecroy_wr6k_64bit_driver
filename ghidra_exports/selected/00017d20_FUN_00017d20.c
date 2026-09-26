
void __thiscall FUN_00017d20(void *this,int param_1,uint param_2)

{
  byte bVar1;
  int iVar2;
  undefined4 uVar3;
  uint uVar4;
  uint uVar5;
  uint uVar6;
  undefined4 local_18;
  
  iVar2 = param_1;
  if ((param_1 == 0) && (uVar3 = FUN_00017b72(0), (char)uVar3 == '\0')) {
    return;
  }
  uVar6 = (uint)*(byte *)(param_1 + 0x78);
  uVar5 = 0x400;
  if (param_2 / uVar6 < 0x400) {
    uVar5 = param_2 / uVar6;
  }
  param_1 = 0;
  if (uVar6 != 0) {
    uVar4 = 0;
    local_18 = 0xe00a5fb;
    do {
      local_18._0_3_ =
           CONCAT12(*(undefined1 *)(uVar4 + *(int *)(iVar2 + 0x18)),(undefined2)local_18);
      FUN_00017c16(this,local_18,*(uint *)(iVar2 + 4),uVar5);
      bVar1 = param_1._3_1_ + 1;
      param_1 = (uint)bVar1 << 0x18;
      uVar4 = (uint)bVar1;
    } while (uVar4 < uVar6);
  }
  FUN_00017cdc(this,iVar2);
  return;
}

