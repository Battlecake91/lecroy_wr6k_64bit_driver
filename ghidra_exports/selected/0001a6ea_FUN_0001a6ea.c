
int __thiscall FUN_0001a6ea(void *this,int param_1)

{
  ushort *puVar1;
  byte *pbVar2;
  byte bVar3;
  undefined4 *puVar4;
  uint uVar5;
  int iVar6;
  int iVar7;
  undefined4 *puVar8;
  undefined4 *puVar9;
  int local_20;
  undefined1 local_1c;
  undefined1 local_1b;
  int local_18;
  uint local_14;
  undefined4 local_10;
  int local_c;
  char local_5;
  
  iVar7 = param_1;
  local_5 = '\0';
  puVar4 = *(undefined4 **)(param_1 + 0x60);
  bVar3 = *(byte *)((int)puVar4 + 1);
  if (0x87 < bVar3) {
switchD_0001a721_caseD_e:
    iVar7 = (**(code **)(*(int *)this + 0xfc))(param_1);
    return iVar7;
  }
  if (bVar3 == 0x87) {
    iVar7 = (**(code **)(*(int *)this + 0xf4))(param_1);
    return iVar7;
  }
  switch(bVar3) {
  case 0:
    if (((*(byte *)((int)this + 0xfc) & 1) != 0) && (*(int *)((int)this + 0x30) != 0)) {
      puVar8 = puVar4;
      puVar9 = puVar4 + -9;
      for (iVar7 = 7; iVar7 != 0; iVar7 = iVar7 + -1) {
        *puVar9 = *puVar8;
        puVar8 = puVar8 + 1;
        puVar9 = puVar9 + 1;
      }
      *(undefined1 *)((int)puVar4 + -0x21) = 0;
      iVar7 = FUN_0001bed2(param_1,1,&local_10);
      if (iVar7 < 0) goto LAB_0001ae18;
    }
    local_c = (**(code **)(*(int *)this + 0x88))(param_1);
    if (-1 < local_c) {
      if (((*(byte *)((int)this + 0x110) & 4) != 0) && ((*(byte *)((int)this + 0x150) & 4) != 0)) {
        FUN_00019e56(this,0,5,0,0);
      }
      if ((*(byte *)((int)this + 0x110) & 0x80) != 0) {
        FUN_00019812((int)this);
      }
    }
    *(uint *)((int)this + 0xc0) =
         *(uint *)((int)this + 0xc0) ^ ((uint)(-1 < local_c) << 3 ^ *(uint *)((int)this + 0xc0)) & 8
    ;
    if ((*(byte *)((int)this + 0x100) & 1) != 0) {
      FUN_00010798(&param_1,(int)this,local_c);
    }
    if (((*(byte *)((int)this + 0xc0) & 8) != 0) && ((*(byte *)((int)this + 200) & 0x40) != 0)) {
      FUN_00019f54(this,0);
    }
    break;
  case 1:
    if ((((*(uint *)((int)this + 0xe8) & 1) != 0) && (*(int *)((int)this + 0x3c) != 0)) ||
       (((*(uint *)((int)this + 0xe8) & 2) != 0 && (2 < *(int *)((int)this + 0x38))))) {
LAB_0001a8be:
      iVar7 = -0x7fffffef;
LAB_0001ae18:
      iVar7 = FUN_00010798(&param_1,(int)this,iVar7);
      return iVar7;
    }
    local_c = (**(code **)(*(int *)this + 0x90))(param_1);
    *(uint *)((int)this + 0xc0) =
         *(uint *)((int)this + 0xc0) ^ ((uint)(-1 < local_c) << 1 ^ *(uint *)((int)this + 0xc0)) & 2
    ;
    if ((*(uint *)((int)this + 0xc0) & 2) != 0) {
      if (((*(byte *)((int)this + 0xe8) & 8) != 0) &&
         (local_14 = *(uint *)(*(int *)((int)this + 4) + 0x14), local_14 != 0)) {
        *(undefined4 *)(local_14 + 0x1c) = 0;
        FUN_00010798(&local_14,(int)this,0xc0000120);
      }
      if ((*(byte *)((int)this + 0xe8) & 4) != 0) {
        local_14 = local_14 & 0xffff0000;
        local_18 = *(int *)((int)this + 4) + 0x60;
        FUN_0001bb4e(&local_18,(int)this,0);
        if (local_18 != 0) {
          FUN_00010598(&local_18);
        }
      }
      if (((*(byte *)((int)this + 0xe8) & 0x10) != 0) && ((*(byte *)((int)this + 0xc0) & 8) != 0)) {
        local_5 = '\x01';
        FUN_0001955a((int)this);
        iVar6 = (**(code **)(*(int *)this + 0x10c))
                          (*(undefined4 *)((int)this + 0xf0),*(undefined4 *)((int)this + 0xf4));
        if ((iVar6 == 0x102) && ((*(byte *)((int)this + 0xe8) & 0x40) != 0)) {
          FUN_0001a276(this,0);
          local_1c = 0;
          local_1b = 0;
          local_20 = *(int *)((int)this + 4) + 0x60;
          FUN_0001bb4e(&local_20,(int)this,0);
          local_14 = *(uint *)(*(int *)((int)this + 4) + 0x14);
          if (local_14 != 0) {
            *(undefined4 *)(local_14 + 0x1c) = 0;
            FUN_00010798(&local_14,(int)this,0xc0000120);
          }
          if (local_20 != 0) {
            FUN_00010598(&local_20);
          }
        }
      }
    }
    if (((*(byte *)((int)this + 0xfc) & 0x20) == 0) && ((*(byte *)((int)this + 0x100) & 4) == 0)) {
      return local_c;
    }
    if (((*(byte *)((int)this + 0xc0) & 2) != 0) && (*(int *)((int)this + 0x30) != 0)) {
      *(undefined4 *)(iVar7 + 0x18) = 0;
LAB_0001adbe:
      *(char *)(iVar7 + 0x23) = *(char *)(iVar7 + 0x23) + '\x01';
      *(int *)(iVar7 + 0x60) = *(int *)(iVar7 + 0x60) + 0x24;
      if (local_5 != '\0') {
        iVar7 = IofCallDriver();
        return iVar7;
      }
LAB_0001aeb2:
      iVar7 = FUN_000107e0((int)this);
      return iVar7;
    }
    goto LAB_0001ac80;
  case 2:
    KeWaitForSingleObject(*(undefined4 *)((int)this + 0x44),0,0,1,0);
    *(uint *)((int)this + 0xc0) = *(uint *)((int)this + 0xc0) | 0x14;
    if ((*(byte *)((int)this + 0xf8) & 0x10) != 0) {
      (**(code **)(*(int *)this + 0x108))(1);
    }
    if ((((*(byte *)((int)this + 0x110) & 4) != 0) && (*(int *)((int)this + 0x1ac) != 0)) &&
       ((*(byte *)((int)this + 0x150) & 0x10) != 0)) {
      FUN_00019a00((int)this);
    }
    if ((*(byte *)((int)this + 200) & 0x40) != 0) {
      FUN_00019f9a(this,0);
    }
    local_c = (**(code **)(*(int *)this + 0x94))(iVar7);
    if (((*(byte *)((int)this + 0xfc) & 0x40) != 0) || ((*(byte *)((int)this + 0x100) & 0x40) != 0))
    {
      FUN_0001955a((int)this);
    }
    if ((*(byte *)((int)this + 200) & 8) != 0) {
      (**(code **)(*(int *)this + 0x10c))
                (*(undefined4 *)((int)this + 0xd0),*(undefined4 *)((int)this + 0xd4));
    }
    if ((((*(byte *)((int)this + 0xfc) & 0x40) != 0) || ((*(byte *)((int)this + 0x100) & 0x40) != 0)
        ) && (*(int *)((int)this + 0x30) != 0)) {
      *(undefined4 *)(iVar7 + 0x18) = 0;
      *(char *)(iVar7 + 0x23) = *(char *)(iVar7 + 0x23) + '\x01';
      *(int *)(iVar7 + 0x60) = *(int *)(iVar7 + 0x60) + 0x24;
      local_c = IofCallDriver();
    }
    if (((*(byte *)((int)this + 200) & 0x10) != 0) && (*(int *)((int)this + 0x30) != 0)) {
      IoDetachDevice(*(undefined4 *)(*(int *)((int)this + 0x30) + 4));
      *(undefined4 *)((int)this + 0x30) = 0;
    }
    FUN_0001955a((int)this);
    if ((*(byte *)((int)this + 200) & 8) != 0) {
      (**(code **)(*(int *)this + 0x110))
                (*(undefined4 *)((int)this + 0xd0),*(undefined4 *)((int)this + 0xd4));
    }
    puVar1 = (ushort *)((int)this + 0x18);
    if ((*puVar1 & 0xfffe) != 0) {
      IoDeleteSymbolicLink(puVar1);
      FUN_000105cc(puVar1);
    }
    if ((*(byte *)(*(int *)((int)this + 4) + 0x1d) & 8) != 0) {
      FUN_0001d3f2();
      pbVar2 = (byte *)(*(int *)((int)this + 4) + 0x1d);
      *pbVar2 = *pbVar2 & 0xf7;
    }
    if ((*(uint *)((int)this + 200) & 0x20) == 0) {
      *(undefined4 *)((int)this + 4) = 0;
    }
    if (((*(uint *)((int)this + 200) & 4) == 0) || (*(int *)((int)this + 0x3c) != 0)) {
      *(uint *)((int)this + 0xc0) = *(uint *)((int)this + 0xc0) & 0xffffffe4;
      KeReleaseMutex(*(undefined4 *)((int)this + 0x44),0);
    }
    else {
      KeReleaseMutex(*(undefined4 *)((int)this + 0x44),0);
      (*(code *)**(undefined4 **)this)();
    }
    break;
  case 3:
    if (((*(byte *)((int)this + 0xfc) & 4) != 0) && (*(int *)((int)this + 0x30) != 0)) {
      puVar8 = puVar4;
      puVar9 = puVar4 + -9;
      for (iVar7 = 7; iVar7 != 0; iVar7 = iVar7 + -1) {
        *puVar9 = *puVar8;
        puVar8 = puVar8 + 1;
        puVar9 = puVar9 + 1;
      }
      *(undefined1 *)((int)puVar4 + -0x21) = 0;
      iVar7 = FUN_0001bed2(param_1,1,&local_10);
      if (iVar7 < 0) goto LAB_0001ae18;
    }
    local_c = (**(code **)(*(int *)this + 0x98))(param_1);
    *(uint *)((int)this + 0xc0) = *(uint *)((int)this + 0xc0) & 0xfffffffd;
    if ((*(byte *)((int)this + 0xf8) & 0x80) != 0) {
      (**(code **)(*(int *)this + 0x108))(0);
    }
    bVar3 = *(byte *)((int)this + 0x100) & 0x10;
    goto LAB_0001ac7a;
  case 4:
    if ((*(byte *)((int)this + 0xf8) & 0x20) != 0) {
      FUN_0001a276(this,0);
    }
    if ((((*(byte *)((int)this + 0x110) & 4) != 0) && (*(int *)((int)this + 0x1ac) != 0)) &&
       ((*(byte *)((int)this + 0x150) & 8) != 0)) {
      FUN_00019a00((int)this);
    }
    local_c = (**(code **)(*(int *)this + 0x8c))(iVar7);
    if ((((*(byte *)((int)this + 0xfc) & 0x10) != 0) || ((*(byte *)((int)this + 0x100) & 2) != 0))
       && (*(int *)((int)this + 0x30) != 0)) {
      uVar5 = *(uint *)((int)this + 0xc0);
      if ((uVar5 & 8) == 0) {
        local_c = FUN_00010798(&param_1,(int)this,0);
      }
      else {
        *(uint *)((int)this + 0xc0) = uVar5 & 0xfffffff7;
        *(char *)(iVar7 + 0x23) = *(char *)(iVar7 + 0x23) + '\x01';
        *(int *)(iVar7 + 0x60) = *(int *)(iVar7 + 0x60) + 0x24;
        local_c = FUN_000107e0((int)this);
      }
    }
    *(uint *)((int)this + 0xc0) = *(uint *)((int)this + 0xc0) & 0xfffffff6;
    if ((*(byte *)((int)this + 200) & 0x40) != 0) {
      FUN_00019f9a(this,0);
    }
    break;
  case 5:
    if ((((*(uint *)((int)this + 0xd8) & 1) != 0) && (*(int *)((int)this + 0x3c) != 0)) ||
       (((*(uint *)((int)this + 0xd8) & 2) != 0 && (2 < *(int *)((int)this + 0x38)))))
    goto LAB_0001a8be;
    local_c = (**(code **)(*(int *)this + 0x9c))(param_1);
    *(uint *)((int)this + 0xc0) =
         *(uint *)((int)this + 0xc0) ^ ((uint)(-1 < local_c) ^ *(uint *)((int)this + 0xc0)) & 1;
    if ((*(uint *)((int)this + 0xc0) & 1) != 0) {
      if (((*(byte *)((int)this + 0xd8) & 8) != 0) &&
         (local_14 = *(uint *)(*(int *)((int)this + 4) + 0x14), local_14 != 0)) {
        *(undefined4 *)(local_14 + 0x1c) = 0;
        FUN_00010798(&local_14,(int)this,0xc0000120);
      }
      if ((*(byte *)((int)this + 0xd8) & 4) != 0) {
        local_1c = 0;
        local_1b = 0;
        local_20 = *(int *)((int)this + 4) + 0x60;
        FUN_0001bb4e(&local_20,(int)this,0);
        if (local_20 != 0) {
          FUN_00010598(&local_20);
        }
      }
      if (((*(byte *)((int)this + 0xd8) & 0x10) != 0) && ((*(byte *)((int)this + 0xc0) & 8) != 0)) {
        local_5 = '\x01';
        FUN_0001955a((int)this);
        iVar6 = (**(code **)(*(int *)this + 0x10c))
                          (*(undefined4 *)((int)this + 0xe0),*(undefined4 *)((int)this + 0xe4));
        if ((iVar6 == 0x102) && ((*(byte *)((int)this + 0xd8) & 0x40) != 0)) {
          FUN_0001a276(this,0);
        }
      }
    }
    if (((*(byte *)((int)this + 0xfc) & 8) == 0) && ((*(byte *)((int)this + 0x100) & 8) == 0)) {
      return local_c;
    }
    if ((((*(uint *)((int)this + 0xc0) & 1) != 0) && (*(int *)((int)this + 0x30) != 0)) &&
       ((*(uint *)((int)this + 0xc0) & 8) != 0)) goto LAB_0001adbe;
    goto LAB_0001ac80;
  case 6:
    if (((*(byte *)((int)this + 0xfc) & 2) != 0) && (*(int *)((int)this + 0x30) != 0)) {
      puVar8 = puVar4;
      puVar9 = puVar4 + -9;
      for (iVar7 = 7; iVar7 != 0; iVar7 = iVar7 + -1) {
        *puVar9 = *puVar8;
        puVar8 = puVar8 + 1;
        puVar9 = puVar9 + 1;
      }
      *(undefined1 *)((int)puVar4 + -0x21) = 0;
      iVar7 = FUN_0001bed2(param_1,1,&local_10);
      if (iVar7 < 0) goto LAB_0001ae18;
    }
    local_c = (**(code **)(*(int *)this + 0xa0))(param_1);
    *(uint *)((int)this + 0xc0) = *(uint *)((int)this + 0xc0) & 0xfffffffe;
    if ((*(byte *)((int)this + 0xf8) & 0x40) != 0) {
      (**(code **)(*(int *)this + 0x108))(0);
    }
    bVar3 = *(byte *)((int)this + 0x100) & 0x20;
LAB_0001ac7a:
    if (bVar3 == 0) {
      return local_c;
    }
    goto LAB_0001ac80;
  case 7:
    local_c = (**(code **)(*(int *)this + 0xa4))(param_1);
    break;
  case 8:
    local_c = (**(code **)(*(int *)this + 0xa8))(param_1);
    break;
  case 9:
    local_c = (**(code **)(*(int *)this + 0xac))(param_1);
    break;
  case 10:
    local_c = (**(code **)(*(int *)this + 0xb0))(param_1);
    break;
  case 0xb:
    local_c = (**(code **)(*(int *)this + 0xb4))(param_1);
    break;
  case 0xc:
    local_c = (**(code **)(*(int *)this + 0xe4))(param_1);
    break;
  case 0xd:
    local_c = (**(code **)(*(int *)this + 0xe8))(param_1);
    break;
  default:
    goto switchD_0001a721_caseD_e;
  case 0xf:
    local_c = (**(code **)(*(int *)this + 0xb8))(param_1);
    break;
  case 0x10:
    local_c = (**(code **)(*(int *)this + 0xbc))(param_1);
    break;
  case 0x11:
    local_c = (**(code **)(*(int *)this + 0xc0))(param_1);
    break;
  case 0x12:
    local_c = (**(code **)(*(int *)this + 0xc4))(param_1);
    break;
  case 0x13:
    local_c = (**(code **)(*(int *)this + 200))(param_1);
    break;
  case 0x14:
    local_c = (**(code **)(*(int *)this + 0xcc))(param_1);
    if (local_c < 0) {
      return local_c;
    }
    if (((*(byte *)((int)this + 0xfc) & 0x80) == 0) && ((*(byte *)((int)this + 0x100) & 0x80) == 0))
    {
      return local_c;
    }
    if ((((*(uint *)((int)this + 0xc0) & 1) == 0) && (*(int *)((int)this + 0x30) != 0)) &&
       ((*(uint *)((int)this + 0xc0) & 8) != 0)) {
      *(char *)(iVar7 + 0x23) = *(char *)(iVar7 + 0x23) + '\x01';
      *(int *)(iVar7 + 0x60) = *(int *)(iVar7 + 0x60) + 0x24;
      goto LAB_0001aeb2;
    }
LAB_0001ac80:
    FUN_00010798(&param_1,(int)this,local_c);
    break;
  case 0x15:
    local_c = (**(code **)(*(int *)this + 0xd0))(param_1);
    break;
  case 0x16:
    local_c = (**(code **)(*(int *)this + 0xec))(param_1);
    break;
  case 0x17:
    *(uint *)((int)this + 0xc0) = *(uint *)((int)this + 0xc0) | 0x20;
    if ((*(byte *)((int)this + 0xf8) & 0x10) != 0) {
      (**(code **)(*(int *)this + 0x108))(1);
    }
    local_c = (**(code **)(*(int *)this + 0xf0))(iVar7);
    if ((((*(uint *)((int)this + 0xfc) & 0x100) != 0) ||
        ((*(uint *)((int)this + 0x100) & 0x100) != 0)) && (*(int *)((int)this + 0x30) != 0)) {
      *(char *)(iVar7 + 0x23) = *(char *)(iVar7 + 0x23) + '\x01';
      *(int *)(iVar7 + 0x60) = *(int *)(iVar7 + 0x60) + 0x24;
      local_c = FUN_000107e0((int)this);
    }
    *(uint *)((int)this + 0xc0) = *(uint *)((int)this + 0xc0) & 0xfffffff4;
  }
  return local_c;
}

