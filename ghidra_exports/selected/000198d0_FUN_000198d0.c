
int __thiscall FUN_000198d0(void *this,undefined4 *param_1)

{
  byte bVar1;
  int iVar2;
  int iVar3;
  undefined4 local_14;
  undefined4 local_10;
  undefined4 local_c;
  undefined4 local_8;
  
  bVar1 = KeGetCurrentIrql();
  if (bVar1 < 2) {
    local_14 = 0;
    local_10 = 0;
    local_c = 0;
    local_8 = CONCAT31(local_8._1_3_,
                       *(undefined1 *)(*(int *)(*(int *)((int)this + 0x30) + 4) + 0x30));
    iVar3 = IoAllocateIrp(local_8,0);
    if (iVar3 == 0) {
      iVar2 = -0x3fffff66;
    }
    else {
      *(undefined1 *)(*(int *)(iVar3 + 0x60) + -0x24) = 0x16;
      *(undefined1 *)(*(int *)(iVar3 + 0x60) + -0x23) = 1;
      *(undefined4 **)(*(int *)(iVar3 + 0x60) + -0x20) = &local_14;
      iVar2 = FUN_0001ba0a(*(void **)((int)this + 0x30),iVar3,1,(undefined4 *)0x0);
      if (iVar2 == 0) {
        *param_1 = local_14;
        param_1[1] = local_10;
        param_1[2] = local_c;
      }
      IoFreeIrp(iVar3);
    }
  }
  else {
    iVar2 = -0x3fffff45;
  }
  return iVar2;
}

