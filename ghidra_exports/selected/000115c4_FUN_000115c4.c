
int __thiscall FUN_000115c4(void *this,int param_1)

{
  uint *puVar1;
  int iVar2;
  undefined4 uVar3;
  int local_3c [9];
  undefined2 local_18 [2];
  int local_14;
  undefined1 local_10;
  uint *local_c;
  uint local_8;
  
  *(undefined4 *)(param_1 + 0x1c) = 0;
  puVar1 = *(uint **)(*(int *)(param_1 + 0x60) + 8);
  local_3c[0] = *(int *)((int)this + 0x14a9);
  local_8 = 0;
  local_c = puVar1;
  FUN_0001888e(local_3c);
  uVar3 = 1;
  iVar2 = FUN_00018812(local_3c,0);
  param_1 = FUN_000106e6((void *)((int)this + 0x14b5),puVar1,iVar2,uVar3);
  if (-1 < param_1) {
    uVar3 = 1;
    iVar2 = FUN_00018812(local_3c,1);
    param_1 = FUN_000106e6((void *)((int)this + 0x14d5),puVar1,iVar2,uVar3);
    if (-1 < param_1) {
      uVar3 = 1;
      iVar2 = FUN_00018812(local_3c,2);
      param_1 = FUN_000106e6((void *)((int)this + 0x14f5),puVar1,iVar2,uVar3);
      if (-1 < param_1) {
        FUN_00014847((void *)((int)this + 0x1e0),(int)this + 0x14b5,(int)this + 0x14d5,
                     (int)this + 0x14f5);
        iVar2 = FUN_00010380(1);
        if (iVar2 == 0) {
          iVar2 = 0;
        }
        else {
          local_8 = 1;
          local_10 = 0;
          RtlInitUnicodeString(local_18,L"regTrace");
        }
        FUN_00010b10((void *)((int)this + 0x394),iVar2);
        if (((local_8 & 1) != 0) && (local_8 = local_8 & 0xfffffffe, local_14 != 0)) {
          FUN_000105cc(local_18);
        }
        iVar2 = FUN_00010380(1);
        if (iVar2 == 0) {
          iVar2 = 0;
        }
        else {
          local_8 = local_8 | 2;
          local_10 = 0;
          RtlInitUnicodeString(local_18,L"regTrace");
        }
        FUN_00010b10(&DAT_0001ce24,iVar2);
        if (((local_8 & 2) != 0) && (local_8 = local_8 & 0xfffffffd, local_14 != 0)) {
          FUN_000105cc(local_18);
        }
        iVar2 = FUN_00010380(1);
        if (iVar2 == 0) {
          iVar2 = 0;
        }
        else {
          local_8 = local_8 | 4;
          local_10 = 0;
          RtlInitUnicodeString(local_18,L"regTrace");
        }
        FUN_00010b10((void *)((int)this + 0x3bc),iVar2);
        if (((local_8 & 4) != 0) && (local_8 = local_8 & 0xfffffffb, local_14 != 0)) {
          FUN_000105cc(local_18);
        }
        iVar2 = FUN_00010380(1);
        if (iVar2 == 0) {
          iVar2 = 0;
        }
        else {
          local_8 = local_8 | 8;
          local_10 = 0;
          RtlInitUnicodeString(local_18,L"regTrace");
        }
        FUN_00010b10((void *)((int)this + 0x3e4),iVar2);
        if (((local_8 & 8) != 0) && (local_8 = local_8 & 0xfffffff7, local_14 != 0)) {
          FUN_000105cc(local_18);
        }
        iVar2 = FUN_00010380(1);
        if (iVar2 == 0) {
          iVar2 = 0;
        }
        else {
          local_8 = local_8 | 0x10;
          local_10 = 0;
          RtlInitUnicodeString(local_18,L"regTrace");
        }
        FUN_00010b10((void *)((int)this + 0x344),iVar2);
        if (((local_8 & 0x10) != 0) && (local_8 = local_8 & 0xffffffef, local_14 != 0)) {
          FUN_000105cc(local_18);
        }
        iVar2 = FUN_00010380(1);
        if (iVar2 == 0) {
          iVar2 = 0;
        }
        else {
          local_8 = local_8 | 0x20;
          local_10 = 0;
          RtlInitUnicodeString(local_18,L"regTrace");
        }
        FUN_00010b10((void *)((int)this + 0x36c),iVar2);
        if (((local_8 & 0x20) != 0) && (local_14 != 0)) {
          FUN_000105cc(local_18);
        }
        iVar2 = FUN_0001060c(&PTR_PTR_0001ccd8,local_c,&LAB_00010b30,this,0,0);
        if (iVar2 < 0) {
          FUN_0001082e((int)this);
        }
        else {
          KeInitializeDpc((int)this + 0x1515,&LAB_0001151e,this);
        }
        FUN_00018862((int)local_3c);
        return iVar2;
      }
    }
  }
  FUN_0001082e((int)this);
  FUN_00018862((int)local_3c);
  return param_1;
}

