
int __thiscall FUN_00011f54(void *this,int param_1)

{
  byte bVar1;
  int iVar2;
  uint *puVar3;
  uint uVar4;
  int iVar5;
  byte *pbVar6;
  int iVar7;
  uint local_c;
  byte *local_8;
  
  iVar2 = param_1;
  local_8 = *(byte **)(param_1 + 0xc);
  if (local_8 == (byte *)0x0) {
    *(undefined4 *)(param_1 + 0x18) = 0xc000000d;
    *(undefined4 *)((int)this + 0x1272) = 3;
    puVar3 = FUN_00010750((uint *)((int)this + 0x1256));
    FUN_00010750(puVar3);
    iVar5 = *(int *)(param_1 + 0x18);
  }
  else {
    uVar4 = *(uint *)(*(int *)(param_1 + 0x60) + 8);
    if ((uVar4 == 0) || (0x200 < uVar4)) {
      *(undefined4 *)((int)this + 0x1272) = 3;
      puVar3 = FUN_00010750((uint *)((int)this + 0x1256));
      FUN_00010750(puVar3);
      iVar5 = -0x3ffffdfa;
    }
    else {
      local_c = 0;
      param_1 = 3;
LAB_00011fcc:
      do {
        uVar4 = *(uint *)(*(int *)(iVar2 + 0x60) + 8);
        if ((local_c & 0xffff) < uVar4) {
          uVar4 = uVar4 - (local_c & 0xffff);
          if (0x1f < uVar4) {
            uVar4 = 0x20;
          }
          iVar5 = FUN_00016d90((void *)((int)this + 0x12ab),local_8,uVar4,local_c);
          if (iVar5 == 0) {
            local_c = local_c + 0x20;
            local_8 = local_8 + 0x20;
            goto LAB_00011fcc;
          }
        }
        pbVar6 = (byte *)ExAllocatePoolWithTag
                                   (0,*(undefined4 *)(*(int *)(iVar2 + 0x60) + 8),0x4443654c);
        iVar5 = FUN_00016f2c((void *)((int)this + 0x12ab),pbVar6,
                             *(int *)(*(int *)(iVar2 + 0x60) + 8));
        iVar7 = RtlCompareMemory(pbVar6,*(undefined4 *)(iVar2 + 0xc),
                                 *(undefined4 *)(*(int *)(iVar2 + 0x60) + 8));
        if (iVar7 != *(int *)(*(int *)(iVar2 + 0x60) + 8)) {
          if (pbVar6 != (byte *)0x0) {
            ExFreePool(pbVar6);
            pbVar6 = (byte *)0x0;
          }
          FUN_00018a1c((uint *)((int)this + 0x1256),2,
                       "IOCTL_WRITE_DALLAS_MEMORY_Handler: readback buffer different 0x%x\n");
          iVar5 = -0x3fffffff;
        }
        if (pbVar6 != (byte *)0x0) {
          ExFreePool(pbVar6);
        }
        if (iVar5 == 0) break;
        bVar1 = (char)param_1 - 1;
        param_1 = (int)bVar1;
      } while ('\0' < (char)bVar1);
      *(int *)(iVar2 + 0x18) = iVar5;
      if (iVar5 == 0) {
        *(undefined4 *)(iVar2 + 0x1c) = 0;
      }
    }
  }
  return iVar5;
}

