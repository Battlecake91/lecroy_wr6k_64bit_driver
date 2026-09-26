
int __thiscall FUN_00013dc6(void *this,uint param_1,byte *param_2)

{
  int *this_00;
  uint *puVar1;
  byte bVar2;
  uint uVar3;
  uint uVar4;
  int iVar5;
  uint uVar6;
  undefined4 uVar7;
  byte *pbVar8;
  undefined1 local_98 [124];
  int local_1c;
  undefined4 local_18;
  int local_10;
  int *local_c;
  uint local_8;
  
  uVar4 = param_1;
  if (DAT_0001cd08 == 0) {
    DAT_0001cd08 = 0x11;
    local_1c = *(int *)(param_1 + 0x3c);
    local_18 = *(undefined4 *)(*(int *)(param_1 + 0x60) + 4);
    local_10 = (**(code **)(*(int *)this + 0xc))(&local_1c,(int)this + 0xe98,1,1);
    this_00 = (int *)((int)this + 0x100);
    local_c = this_00;
    iVar5 = FUN_00018168(this_00,local_10,(int *)0x0);
    *this_00 = iVar5;
    if (iVar5 == 0) {
      DAT_0001cd08 = 0;
      iVar5 = -0x3ffffff3;
    }
    else {
      bVar2 = *param_2;
      uVar6 = (uint)bVar2;
      pbVar8 = param_2 + 1;
      FUN_00017ddc(local_98,uVar6);
      uVar7 = FUN_00017b72((int)local_98);
      if ((char)uVar7 == '\0') {
        iVar5 = -0x3fffff66;
      }
      else {
        if (bVar2 != 0) {
          param_1 = 1;
          local_8 = uVar6;
          do {
            FUN_00017ee0(local_98,(uint)pbVar8[1],param_1 == uVar6);
            pbVar8 = pbVar8 + 2;
            param_1 = param_1 + 1;
            local_8 = local_8 - 1;
          } while (local_8 != 0);
        }
        uVar3 = *(uint *)(pbVar8 + 4);
        uVar6 = *(int *)pbVar8 * uVar6 * uVar3;
        if (((uVar3 < 0x401) || ((uVar6 & 0x3ff) == 0)) && (uVar6 < 0x1000000)) {
          if (uVar6 - *(int *)(*local_c + 4) == 0) {
            puVar1 = (uint *)(*(int *)(*(int *)(uVar4 + 0x60) + 4) + -4 + local_1c);
            *puVar1 = 0;
            iVar5 = FUN_00012d6a(this,(int)local_98,uVar6,puVar1,'\0',uVar3);
            *(uint *)(uVar4 + 0x1c) = *puVar1 + 4;
            (**(code **)(*(int *)this + 8))(&local_10);
            *local_c = 0;
          }
          else {
            iVar5 = -0x3ffffdfa;
          }
        }
        else {
          iVar5 = -0x3ffffff3;
        }
      }
      DAT_0001cd08 = 0;
      FUN_00017e58((int)local_98);
    }
  }
  else {
    iVar5 = -0x3fffff5d;
  }
  return iVar5;
}

