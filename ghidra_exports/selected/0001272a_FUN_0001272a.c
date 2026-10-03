
undefined4 __thiscall FUN_0001272a(void *this,int param_1)

{
  uint uVar1;
  uint *puVar2;
  uint uVar3;
  int iVar4;
  byte bVar5;
  uint uVar6;
  undefined3 uStack_13;
  undefined1 uStack_10;
  undefined3 local_f;
  
  puVar2 = *(uint **)(param_1 + 0xc);
  if (puVar2 == (uint *)0x0) {
    *(undefined4 *)(param_1 + 0x1c) = 0;
    *(undefined4 *)(param_1 + 0x18) = 0xc000000d;
  }
  iVar4 = *(int *)(*(int *)(param_1 + 0x60) + 8);
  if (iVar4 == 9) {
    uVar6 = *puVar2;
    uVar1 = puVar2[1];
    uVar3 = puVar2[2];
    bVar5 = (byte)uVar6;
    if (2 < bVar5) {
      return 0xc000000d;
    }
    if ((bVar5 != 0) && (DAT_0001cd08 == -1)) {
      *(undefined4 *)(param_1 + 0x1c) = 0;
      *(undefined4 *)(param_1 + 0x18) = 0xc00000a3;
      return 0xc00000a3;
    }
    if ((uVar6 & 0x300) != 0) {
      *(undefined4 *)(param_1 + 0x1c) = 0;
      *(undefined4 *)(param_1 + 0x18) = 0xc000000d;
    }
    uStack_13 = (undefined3)(uVar6 >> 8);
    uStack_10 = (undefined1)uVar1;
    local_f = (undefined3)(uVar1 >> 8);
    if ((CONCAT13(uStack_10,uStack_13) != 0x84) || (bVar5 != 0)) {
      iVar4 = (**(code **)(*(int *)this + 0x1c))(uVar6);
      WRITE_REGISTER_ULONG
                (*(int *)(iVar4 + 0x10) + CONCAT13(uStack_10,uStack_13),
                 CONCAT13((char)uVar3,local_f));
LAB_00012813:
      (**(code **)(*(int *)this + 0x1c))(uVar6);
      goto LAB_0001281b;
    }
    uVar1 = CONCAT13((char)uVar3,local_f);
  }
  else {
    if (iVar4 != 8) {
      return 0xc0000206;
    }
    uVar6 = *puVar2;
    uVar1 = puVar2[1];
    if ((uVar6 & 3) != 0) {
      *(undefined4 *)(param_1 + 0x1c) = 0;
      *(undefined4 *)(param_1 + 0x18) = 0xc000000d;
    }
    if (uVar6 != 0x84) {
      iVar4 = (**(code **)(*(int *)this + 0x1c))(0);
      WRITE_REGISTER_ULONG(*(int *)(iVar4 + 0x10) + uVar6,uVar1);
      uVar6 = 0;
      goto LAB_00012813;
    }
  }
  DAT_0001ce44 = uVar1;
  WRITE_REGISTER_ULONG(DAT_0001ce20,DAT_0001ce44);
LAB_0001281b:
  *(undefined4 *)(param_1 + 0x1c) = 0;
  *(undefined4 *)(param_1 + 0x18) = 0;
  return 0;
}

