
int __thiscall FUN_00013c84(void *this,int param_1,uint param_2,byte *param_3)

{
  int *this_00;
  byte bVar1;
  uint uVar2;
  uint *puVar3;
  int iVar4;
  undefined4 uVar5;
  uint uVar6;
  byte *pbVar7;
  byte *pbVar8;
  undefined1 local_84 [124];
  void *local_8;
  
  if (DAT_0001cd08 != 0) {
    return -0x3fffff5d;
  }
  this_00 = (int *)((int)this + 0x100);
  DAT_0001cd08 = 0x11;
  local_8 = this;
  iVar4 = FUN_00018168(this_00,param_2,(int *)0x0);
  *this_00 = iVar4;
  if (iVar4 == 0) {
    DAT_0001cd08 = 0;
    return -0x3ffffff3;
  }
  bVar1 = *param_3;
  pbVar8 = (byte *)(uint)bVar1;
  pbVar7 = param_3 + 1;
  FUN_00017ddc(local_84,(uint)pbVar8);
  uVar5 = FUN_00017b72((int)local_84);
  if ((char)uVar5 == '\0') {
    iVar4 = -0x3fffff66;
    goto LAB_00013dae;
  }
  if (bVar1 != 0) {
    param_2 = 1;
    param_3 = pbVar8;
    do {
      FUN_00017ee0(local_84,(uint)pbVar7[1],(byte *)param_2 == pbVar8);
      pbVar7 = pbVar7 + 2;
      param_2 = param_2 + 1;
      param_3 = param_3 + -1;
    } while (param_3 != (byte *)0x0);
  }
  FUN_00017a0a(local_84,*(undefined4 *)pbVar7);
  uVar2 = *(uint *)(pbVar7 + 4);
  uVar6 = (int)pbVar8 * uVar2;
  if (uVar6 < 0x400) {
LAB_00013d5d:
    if (uVar2 % (uint)pbVar8 == 0) {
LAB_00013d67:
      if ((uVar2 < 0x1000000) && (uVar2 % (uint)pbVar8 == 0)) {
        if (uVar2 == *(uint *)(*this_00 + 4)) {
          puVar3 = *(uint **)(param_1 + 0xc);
          *puVar3 = 0;
          iVar4 = FUN_00012d6a(local_8,(int)local_84,uVar2,puVar3,'\x01',0);
          *(undefined4 *)(param_1 + 0x1c) = 4;
          *this_00 = 0;
        }
        else {
          iVar4 = -0x3ffffdfa;
        }
        goto LAB_00013dae;
      }
    }
  }
  else if ((uVar6 & 0x3ff) == 0) {
    if (uVar6 < 0x400) goto LAB_00013d5d;
    goto LAB_00013d67;
  }
  iVar4 = -0x3ffffff3;
LAB_00013dae:
  DAT_0001cd08 = 0;
  FUN_00017e58((int)local_84);
  return iVar4;
}

