
uint __thiscall FUN_0001b096(void *this,int param_1)

{
  byte *pbVar1;
  char cVar2;
  int iVar3;
  int iVar4;
  uint uVar5;
  int iVar6;
  int iVar7;
  undefined4 *puVar8;
  undefined4 *puVar9;
  bool bVar10;
  undefined4 *puVar11;
  uint local_24;
  uint local_20;
  uint local_1c;
  int local_18;
  uint local_14;
  int local_10;
  undefined4 *local_c;
  char local_5;
  
  iVar7 = param_1;
  local_5 = '\0';
  if ((*(uint *)((int)this + 0xc0) & 0x24) != 0) {
    PoStartNextPowerIrp(param_1);
    *(undefined4 *)(iVar7 + 0x1c) = 0;
    puVar11 = (undefined4 *)0xc0000056;
    goto LAB_0001b97d;
  }
  cVar2 = *(char *)(*(int *)(param_1 + 0x60) + 1);
  if (cVar2 == '\0') {
    uVar5 = (**(code **)(*(int *)this + 0xd4))(param_1);
    if ((*(uint *)((int)this + 0x110) & 1) == 0) {
      return uVar5;
    }
    if ((*(byte *)((int)this + 0x138) & 4) == 0) {
      return uVar5;
    }
    if (*(int *)((int)this + 0x30) == 0) {
      return uVar5;
    }
    if ((*(uint *)((int)this + 0x110) & 4) == 0) {
      PoStartNextPowerIrp(iVar7);
      *(undefined4 *)(iVar7 + 0x1c) = 0;
      puVar11 = (undefined4 *)0xc00000bb;
    }
    else if ((*(byte *)((int)this + 0x10c) & 1) == 0) {
      puVar11 = *(undefined4 **)(iVar7 + 0x60);
      if ((*(int *)((int)this + 0x1a4) <= *(int *)((int)this + puVar11[1] * 4 + 0x174)) &&
         (*(int *)((int)this + 0x1a4) <= *(int *)((int)this + 0x194))) {
        puVar8 = puVar11;
        puVar9 = puVar11 + -9;
        for (iVar7 = 7; iVar7 != 0; iVar7 = iVar7 + -1) {
          *puVar9 = *puVar8;
          puVar8 = puVar8 + 1;
          puVar9 = puVar9 + 1;
        }
        *(undefined1 *)((int)puVar11 + -0x21) = 0;
        FUN_000193d0(&param_1,0x19c44,0,'\x01','\x01','\x01');
        PoStartNextPowerIrp(param_1);
        uVar5 = FUN_000107c2(*(void **)((int)this + 0x30),(int)this,param_1);
        if (uVar5 == 0x103) {
          *(uint *)((int)this + 0x10c) = *(uint *)((int)this + 0x10c) | 1;
          return 0x103;
        }
        return uVar5;
      }
      PoStartNextPowerIrp(iVar7);
      *(undefined4 *)(iVar7 + 0x1c) = 0;
      puVar11 = (undefined4 *)0xc0000184;
    }
    else {
      PoStartNextPowerIrp(iVar7);
      *(undefined4 *)(iVar7 + 0x1c) = 0;
      puVar11 = (undefined4 *)0x80000011;
    }
    goto LAB_0001b97d;
  }
  if (cVar2 == '\x01') {
    uVar5 = (**(code **)(*(int *)this + 0xd8))();
    return uVar5;
  }
  if (cVar2 != '\x02') {
    if (cVar2 != '\x03') {
      uVar5 = (**(code **)(*(int *)this + 0x100))();
      return uVar5;
    }
    local_c = (undefined4 *)(**(code **)(*(int *)this + 0xe0))();
    if ((*(uint *)((int)this + 0x110) & 1) == 0) {
      return (uint)local_c;
    }
    if ((*(byte *)((int)this + 0x138) & 2) == 0) {
      return (uint)local_c;
    }
    iVar6 = *(int *)(iVar7 + 0x60);
    if (*(int *)(iVar6 + 8) == 0) {
      if (*(int *)((int)this + 0x1a8) < *(int *)(iVar6 + 0xc)) {
        if ((((*(uint *)((int)this + 0x110) & 4) == 0) || (*(int *)((int)this + 0x1ac) == 0)) ||
           (*(int *)(iVar6 + 0xc) <= *(int *)((int)this + 400))) {
          if ((((*(uint *)((int)this + 0x118) & 1) == 0) || (*(int *)((int)this + 0x3c) == 0)) &&
             (((*(uint *)((int)this + 0x118) & 2) == 0 || (*(int *)((int)this + 0x38) < 3)))) {
            iVar6 = (**(code **)(*(int *)this + 0x124))(*(undefined4 *)(iVar6 + 0xc));
            if (iVar6 == 4) {
              uVar5 = *(uint *)((int)this + 0x10c) & 0xfffffffb | 2;
LAB_0001b206:
              *(uint *)((int)this + 0x10c) = uVar5;
            }
            else if ((iVar6 == 3) || (iVar6 == 2)) {
              uVar5 = *(uint *)((int)this + 0x10c) & 0xfffffffd | 4;
              goto LAB_0001b206;
            }
            if (((*(byte *)((int)this + 0x118) & 0x10) != 0) && (*(int *)((int)this + 0x1a4) == 1))
            {
              local_5 = '\x01';
              FUN_0001955a((int)this);
              iVar6 = FUN_000195c2(this,*(int *)((int)this + 0x120),*(int *)((int)this + 0x124));
              if ((iVar6 == 0x102) && ((*(byte *)((int)this + 0x118) & 0x40) != 0)) {
                FUN_0001a276(this,0);
                local_1c = local_1c & 0xffff0000;
                local_20 = *(int *)((int)this + 4) + 0x60;
                FUN_0001bb4e(&local_20,(int)this,0);
                local_14 = *(uint *)(*(int *)((int)this + 4) + 0x14);
                if (local_14 != 0) {
                  *(undefined4 *)(local_14 + 0x1c) = 0;
                  FUN_00010798(&local_14,(int)this,0xc0000120);
                }
                if (local_20 != 0) {
                  FUN_00010598((int *)&local_20);
                }
              }
            }
          }
          else {
            local_c = (undefined4 *)0x80000011;
          }
        }
        else {
          local_c = (undefined4 *)0xc0000184;
        }
      }
    }
    else if (*(int *)(iVar6 + 8) == 1) {
      iVar6 = *(int *)(iVar6 + 0xc);
      if (iVar6 != *(int *)((int)this + 0x1a4)) {
        if (iVar6 < *(int *)((int)this + 0x1a4)) {
          *(uint *)((int)this + 0x10c) = *(uint *)((int)this + 0x10c) & 0xfffffff9;
        }
        else {
          if (iVar6 == 4) {
            uVar5 = *(uint *)((int)this + 0x10c) & 0xfffffffb | 2;
          }
          else {
            if ((iVar6 != 3) && (iVar6 != 2)) goto LAB_0001b29d;
            uVar5 = *(uint *)((int)this + 0x10c) & 0xfffffffd | 4;
          }
          *(uint *)((int)this + 0x10c) = uVar5;
        }
      }
    }
LAB_0001b29d:
    PoStartNextPowerIrp(iVar7);
    puVar11 = local_c;
    if (local_c != (undefined4 *)0x0) {
LAB_0001b97d:
      uVar5 = FUN_00010798(&param_1,(int)this,puVar11);
      return uVar5;
    }
    puVar11 = *(undefined4 **)(param_1 + 0x60);
    puVar8 = puVar11;
    puVar9 = puVar11 + -9;
    for (iVar7 = 7; iVar7 != 0; iVar7 = iVar7 + -1) {
      *puVar9 = *puVar8;
      puVar8 = puVar8 + 1;
      puVar9 = puVar9 + 1;
    }
    *(undefined1 *)((int)puVar11 + -0x21) = 0;
    iVar7 = param_1;
    if (local_5 != '\0') {
LAB_0001b2d5:
      uVar5 = PoCallDriver(*(undefined4 *)(*(int *)((int)this + 0x30) + 4),param_1);
      return uVar5;
    }
    goto LAB_0001b35e;
  }
  uVar5 = (**(code **)(*(int *)this + 0xdc))();
  iVar6 = param_1;
  if ((*(uint *)((int)this + 0x110) & 1) == 0) {
    return uVar5;
  }
  if ((*(byte *)((int)this + 0x138) & 1) == 0) {
    return uVar5;
  }
  iVar3 = *(int *)(iVar7 + 0x60);
  if (*(int *)(iVar3 + 8) == 0) {
    iVar4 = *(int *)((int)this + 0x1a8);
    if (*(int *)(iVar3 + 0xc) == iVar4) {
      if ((iVar4 == 1) && (*(int *)((int)this + 0x1a4) == 1)) {
        uVar5 = *(uint *)((int)this + 0x10c);
        if (((uVar5 & 6) != 0) &&
           (*(uint *)((int)this + 0x10c) = uVar5 & 0xfffffff9,
           (*(byte *)((int)this + 0x128) & 3) != 0)) {
          (**(code **)(*(int *)this + 0x108))(0);
        }
      }
      if ((*(uint *)((int)this + 0x10c) & 4) != 0) {
        *(uint *)((int)this + 0x10c) = *(uint *)((int)this + 0x10c) & 0xfffffffb;
      }
      if ((*(uint *)((int)this + 0x10c) & 2) != 0) {
        *(uint *)((int)this + 0x10c) = *(uint *)((int)this + 0x10c) & 0xfffffffd;
      }
    }
    else {
      if (*(int *)(iVar3 + 0xc) < iVar4) {
        (**(code **)(*(int *)this + 0x11c))(param_1);
        puVar11 = *(undefined4 **)(iVar6 + 0x60);
        puVar8 = puVar11;
        puVar9 = puVar11 + -9;
        for (iVar7 = 7; iVar7 != 0; iVar7 = iVar7 + -1) {
          *puVar9 = *puVar8;
          puVar8 = puVar8 + 1;
          puVar9 = puVar9 + 1;
        }
        *(undefined1 *)((int)puVar11 + -0x21) = 0;
        FUN_000193d0(&param_1,0x1a410,this,'\x01','\x01','\x01');
        pbVar1 = (byte *)(*(int *)(param_1 + 0x60) + 3);
        *pbVar1 = *pbVar1 | 1;
        PoCallDriver(*(undefined4 *)(*(int *)((int)this + 0x30) + 4),param_1);
        return 0x103;
      }
      (**(code **)(*(int *)this + 0x120))(iVar7);
      *(undefined4 *)((int)this + 0x1a8) = *(undefined4 *)(*(int *)(iVar7 + 0x60) + 0xc);
      local_1c = (**(code **)(*(int *)this + 0x124))(*(undefined4 *)(*(int *)(iVar7 + 0x60) + 0xc));
      if (local_1c != *(uint *)((int)this + 0x1a4)) {
        if (local_1c == 4) {
          uVar5 = *(uint *)((int)this + 0x10c) & 0xfffffffb | 2;
LAB_0001b689:
          *(uint *)((int)this + 0x10c) = uVar5;
        }
        else if ((local_1c == 3) || (local_1c == 2)) {
          uVar5 = *(uint *)((int)this + 0x10c) & 0xfffffffd | 4;
          goto LAB_0001b689;
        }
        if (((*(byte *)((int)this + 0x140) & 8) != 0) &&
           (local_14 = *(uint *)(*(int *)((int)this + 4) + 0x14), local_14 != 0)) {
          *(undefined4 *)(local_14 + 0x1c) = 0;
          FUN_00010798(&local_14,(int)this,0xc0000120);
        }
        if ((*(byte *)((int)this + 0x140) & 4) != 0) {
          local_14 = local_14 & 0xffff0000;
          local_18 = *(int *)((int)this + 4) + 0x60;
          FUN_0001bb4e(&local_18,(int)this,0);
          if (local_18 != 0) {
            FUN_00010598(&local_18);
          }
        }
        if (((*(byte *)((int)this + 0x140) & 0x10) != 0) && (*(int *)((int)this + 0x1a4) == 1)) {
          local_5 = '\x01';
          FUN_0001955a((int)this);
          iVar6 = FUN_000195c2(this,*(int *)((int)this + 0x130),*(int *)((int)this + 0x134));
          if ((iVar6 == 0x102) && ((*(byte *)((int)this + 0x140) & 0x40) != 0)) {
            FUN_0001a276(this,0);
            local_c = (undefined4 *)((uint)local_c & 0xffff0000);
            local_10 = *(int *)((int)this + 4) + 0x60;
            FUN_0001bb4e(&local_10,(int)this,0);
            local_14 = *(uint *)(*(int *)((int)this + 4) + 0x14);
            if (local_14 != 0) {
              *(undefined4 *)(local_14 + 0x1c) = 0;
              FUN_00010798(&local_14,(int)this,0xc0000120);
            }
            if (local_10 != 0) {
              FUN_00010598(&local_10);
            }
          }
        }
        if (((*(byte *)((int)this + 0x110) & 4) == 0) || ((*(byte *)((int)this + 0x150) & 2) == 0))
        {
LAB_0001b7b7:
          if (*(int *)((int)this + 0x1ac) == 0) goto LAB_0001b7d4;
        }
        else if (*(int *)((int)this + 0x1ac) == 0) {
          if (*(int *)(*(int *)(iVar7 + 0x60) + 0xc) <= *(int *)((int)this + 400)) {
            FUN_00019e56(this,0,5,0,0);
          }
          goto LAB_0001b7b7;
        }
        if (*(int *)((int)this + 400) < *(int *)(*(int *)(iVar7 + 0x60) + 0xc)) {
          FUN_00019a00((int)this);
        }
LAB_0001b7d4:
        if (local_5 != '\0') {
          FUN_00019506((int)this);
        }
        local_c = (undefined4 *)ExAllocatePoolWithTag(0,8,0x206d6457);
        if (local_c == (undefined4 *)0x0) {
          local_c = (undefined4 *)0x0;
        }
        else {
          local_c[1] = 0;
        }
        if (local_c != (undefined4 *)0x0) {
          *local_c = this;
          local_c[1] = iVar7;
          pbVar1 = (byte *)(*(int *)(iVar7 + 0x60) + 3);
          *pbVar1 = *pbVar1 | 1;
          uVar5 = PoRequestPowerIrp(*(undefined4 *)((int)this + 0x34),2,local_1c,&LAB_0001a09a,
                                    local_c,0);
          if (-1 < (int)uVar5) {
            return 0x103;
          }
          ExFreePool(local_c);
          PoStartNextPowerIrp(iVar7);
          FUN_00010798(&param_1,(int)this,uVar5);
          return uVar5;
        }
        PoStartNextPowerIrp(iVar7);
        uVar5 = FUN_00010798(&param_1,(int)this,0xc000009a);
        return uVar5;
      }
    }
  }
  else {
    if (*(int *)(iVar3 + 8) != 1) {
      *(char *)(iVar7 + 0x23) = *(char *)(iVar7 + 0x23) + '\x01';
      *(int *)(iVar7 + 0x60) = *(int *)(iVar7 + 0x60) + 0x24;
      PoStartNextPowerIrp(iVar7);
      goto LAB_0001b35e;
    }
    iVar6 = *(int *)(iVar3 + 0xc);
    if (iVar6 == *(int *)((int)this + 0x1a4)) {
      *(uint *)((int)this + 0x10c) = *(uint *)((int)this + 0x10c) & 0xfffffff9;
    }
    else {
      if (iVar6 < *(int *)((int)this + 0x1a4)) {
        if (((*(uint *)((int)this + 0x110) & 2) != 0) &&
           ((*(byte *)((int)this + 0x10c) & 0x10) != 0)) {
          iVar7 = FUN_000198d0(this,&local_24);
          if (iVar7 < 0) {
            *(uint *)((int)this + 0x10c) = *(uint *)((int)this + 0x10c) | 0x20;
          }
          else {
            iVar7 = *(int *)((int)this + 0x1a4);
            if (iVar7 == 2) {
              bVar10 = *(uint *)((int)this + 0x158) < local_24;
            }
            else if (iVar7 == 3) {
              bVar10 = *(uint *)((int)this + 0x15c) < local_20;
            }
            else {
              if (iVar7 != 4) {
                *(uint *)((int)this + 0x10c) = *(uint *)((int)this + 0x10c) & 0xffffffdf;
                goto LAB_0001b3e6;
              }
              bVar10 = *(uint *)((int)this + 0x160) < local_1c;
            }
            *(uint *)((int)this + 0x10c) =
                 *(uint *)((int)this + 0x10c) ^
                 ((uint)bVar10 << 5 ^ *(uint *)((int)this + 0x10c)) & 0x20;
          }
LAB_0001b3e6:
          *(uint *)((int)this + 0x10c) = *(uint *)((int)this + 0x10c) & 0xffffffef;
        }
        puVar11 = *(undefined4 **)(param_1 + 0x60);
        puVar8 = puVar11;
        puVar9 = puVar11 + -9;
        for (iVar7 = 7; iVar7 != 0; iVar7 = iVar7 + -1) {
          *puVar9 = *puVar8;
          puVar8 = puVar8 + 1;
          puVar9 = puVar9 + 1;
        }
        *(undefined1 *)((int)puVar11 + -0x21) = 0;
        FUN_000193d0(&param_1,0x19c54,this,'\x01','\x01','\x01');
        goto LAB_0001b2d5;
      }
      FUN_000104a4(this,&local_1c,iVar6);
      *(uint *)((int)this + 0x10c) = *(uint *)((int)this + 0x10c) & 0xfffffff9;
      (**(code **)(*(int *)this + 0x118))(iVar7);
      if ((*(byte *)((int)this + 0x110) & 2) != 0) {
        iVar6 = FUN_000198d0(this,&local_24);
        if (iVar6 < 0) {
          *(uint *)((int)this + 0x10c) = *(uint *)((int)this + 0x10c) & 0xffffffef;
        }
        else {
          *(uint *)((int)this + 0x158) = local_24;
          *(uint *)((int)this + 0x15c) = local_20;
          *(uint *)((int)this + 0x160) = local_1c;
        }
      }
      if ((*(int *)((int)this + 0x1a4) == 4) && ((*(byte *)((int)this + 0x128) & 0x10) != 0)) {
        FUN_0001a276(this,0);
      }
      if (((((*(byte *)((int)this + 0x110) & 4) != 0) &&
           ((*(byte *)((int)this + 0x150) & 0x20) != 0)) && (*(int *)((int)this + 0x1ac) != 0)) &&
         (*(int *)((int)this + 0x194) < *(int *)((int)this + 0x1a4))) {
        FUN_00019a00((int)this);
      }
      if (((*(byte *)((int)this + 0x140) & 8) != 0) &&
         (local_14 = *(uint *)(*(int *)((int)this + 4) + 0x14), local_14 != 0)) {
        *(undefined4 *)(local_14 + 0x1c) = 0;
        FUN_00010798(&local_14,(int)this,0xc0000120);
      }
      if ((*(byte *)((int)this + 0x140) & 4) != 0) {
        local_1c = local_1c & 0xffff0000;
        local_20 = *(int *)((int)this + 4) + 0x60;
        FUN_0001bb4e(&local_20,(int)this,0);
        if (local_20 != 0) {
          FUN_00010598((int *)&local_20);
        }
      }
      if ((((*(byte *)((int)this + 0x110) & 4) != 0) && ((*(byte *)((int)this + 0x150) & 1) != 0))
         && ((*(int *)((int)this + 0x1ac) == 0 &&
             (*(int *)(*(int *)(iVar7 + 0x60) + 0xc) <= *(int *)((int)this + 0x194))))) {
        FUN_00019e56(this,0,5,0,0);
      }
    }
  }
  puVar11 = *(undefined4 **)(param_1 + 0x60);
  puVar8 = puVar11;
  puVar9 = puVar11 + -9;
  for (iVar7 = 7; iVar7 != 0; iVar7 = iVar7 + -1) {
    *puVar9 = *puVar8;
    puVar8 = puVar8 + 1;
    puVar9 = puVar9 + 1;
  }
  *(undefined1 *)((int)puVar11 + -0x21) = 0;
  PoStartNextPowerIrp(param_1);
  iVar7 = param_1;
LAB_0001b35e:
  uVar5 = FUN_000107c2(*(void **)((int)this + 0x30),(int)this,iVar7);
  return uVar5;
}

