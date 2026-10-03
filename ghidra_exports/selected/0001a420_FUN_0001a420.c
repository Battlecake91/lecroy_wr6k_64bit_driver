
LONG __thiscall FUN_0001a420(void *this,LONG *param_1)

{
  byte bVar1;
  byte bVar2;
  code *pcVar3;
  uint uVar4;
  int iVar5;
  LONG LVar6;
  uint uVar7;
  
  bVar1 = *(byte *)param_1[0x18];
  bVar2 = ((byte *)param_1[0x18])[1];
  pcVar3 = (code *)(&PTR_LAB_0001cd10)[bVar1];
  FUN_00019506((int)this);
  if (((*(byte *)((int)this + 0x10c) & 8) != 0) &&
     (*(undefined4 **)((int)this + 0x108) != (undefined4 *)0x0)) {
    **(undefined4 **)((int)this + 0x108) = 0;
  }
  if (bVar1 == 0) {
    KeWaitForSingleObject(*(undefined4 *)((int)this + 0x44),0,0,1,0);
    if ((*(byte *)((int)this + 0xc0) & 0x24) == 0) {
      iVar5 = (*pcVar3)();
      if (((-1 < iVar5) && (InterlockedIncrement(param_1), *(int *)((int)this + 0x1a4) != 1)) &&
         ((*(byte *)((int)this + 0x110) & 0x20) != 0)) {
        FUN_00019ddc(this,2,1,0);
      }
      KeReleaseMutex(*(undefined4 *)((int)this + 0x44),0);
      return iVar5;
    }
    FUN_0001955a((int)this);
    KeReleaseMutex(*(undefined4 *)((int)this + 0x44),0);
LAB_0001a6ce:
    LVar6 = -0x3fffffaa;
LAB_0001a6d3:
    param_1[6] = LVar6;
    IofCompleteRequest();
  }
  else {
    if (bVar1 == 2) {
      KeWaitForSingleObject(*(undefined4 *)((int)this + 0x44),0,0,1,0);
      iVar5 = (*pcVar3)();
      if (-1 < iVar5) {
        InterlockedDecrement(param_1);
      }
      if (*(int *)((int)this + 0x3c) == 0) {
        if ((((*(uint *)((int)this + 0xc0) & 4) != 0) && ((*(byte *)((int)this + 200) & 4) != 0)) &&
           ((*(uint *)((int)this + 0xc0) & 0x10) == 0)) {
          KeReleaseMutex(*(undefined4 *)((int)this + 0x44),0);
          (*(code *)**(undefined4 **)this)();
          return iVar5;
        }
        if ((*(int *)((int)this + 0x1a4) != *(int *)((int)this + 0x114)) &&
           ((*(byte *)((int)this + 0x110) & 0x40) != 0)) {
          FUN_00019ddc(this,2,*(int *)((int)this + 0x114),0);
        }
      }
      KeReleaseMutex(*(undefined4 *)((int)this + 0x44),0);
      return iVar5;
    }
    if ((bVar1 == 0x10) || (bVar1 == 0x12)) {
      if ((*(byte *)((int)this + 0xf8) & 8) != 0) {
        if (param_1[0x18] == 0) {
          iVar5 = 0;
        }
        else {
          iVar5 = *(int *)(param_1[0x18] + 0x18);
        }
        FUN_0001a276(this,iVar5);
      }
    }
    else if ((bVar1 < 0x16) || ((0x17 < bVar1 && (bVar1 != 0x1b)))) {
      uVar4 = *(uint *)((int)this + 0xc0);
      if (((uVar4 & 0x24) != 0) && ((*(byte *)((int)this + 200) & 1) != 0)) goto LAB_0001a4c9;
      uVar7 = uVar4 >> 3 & 1;
      if ((((uVar7 == 0) || (*(int *)((int)this + 0x1a4) != 1)) || ((uVar4 & 3) != 0)) ||
         ((*(byte *)((int)this + 0x10c) & 6) != 0)) {
        if (((((((uVar4 & 1) != 0) && ((*(byte *)((int)this + 0xf8) & 2) != 0)) ||
              (((uVar4 & 2) != 0 && ((*(byte *)((int)this + 0xf8) & 1) != 0)))) ||
             ((uVar7 == 0 && ((*(byte *)((int)this + 0xf8) & 4) != 0)))) ||
            (((*(uint *)((int)this + 0x10c) & 4) != 0 && ((*(byte *)((int)this + 0x128) & 2) != 0)))
            ) || ((((*(uint *)((int)this + 0x10c) & 2) != 0 &&
                   ((*(byte *)((int)this + 0x128) & 1) != 0)) ||
                  ((*(int *)((int)this + 0x1a4) != 1 &&
                   (((*(byte *)((int)this + 0x128) & 0xc) != 0 || (*(int *)((int)this + 0x1a8) != 1)
                    ))))))) {
          FUN_0001955a((int)this);
          LVar6 = (**(code **)(*(int *)this + 0x104))(param_1);
          return LVar6;
        }
        if ((uVar7 == 0) && ((*(byte *)((int)this + 200) & 2) != 0)) {
          FUN_0001955a((int)this);
          LVar6 = -0x3fffffff;
          goto LAB_0001a6d3;
        }
        if ((*(int *)((int)this + 0x1a4) != 1) &&
           (((*(byte *)((int)this + 0x110) & 8) != 0 &&
            (iVar5 = FUN_00019ddc(this,2,1,0), iVar5 != 0)))) {
          return iVar5;
        }
      }
    }
    else if (((*(uint *)((int)this + 0xc0) & 0x24) != 0) &&
            ((((*(uint *)((int)this + 0xc0) & 0x20) == 0 || (bVar1 != 0x1b)) || (bVar2 != 2)))) {
LAB_0001a4c9:
      FUN_0001955a((int)this);
      goto LAB_0001a6ce;
    }
    LVar6 = (*pcVar3)(param_1);
  }
  return LVar6;
}

