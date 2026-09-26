
void FUN_00011390(int param_1)

{
  int iVar1;
  char cVar2;
  bool bVar3;
  undefined4 uVar4;
  
  cVar2 = FUN_00010636(&PTR_PTR_0001ccd8,&LAB_0001085e,param_1);
  if (cVar2 != '\0') {
    FUN_00010816((undefined4 *)(param_1 + 0x12ee));
  }
  cVar2 = FUN_00010636(&PTR_PTR_0001ccd8,&LAB_00010872,param_1);
  if ((cVar2 != '\0') && (*(int *)(param_1 + 0x2e0) != 0)) {
    KeSetEvent(*(int *)(param_1 + 0x2e0) + 0x24,0,0);
  }
  cVar2 = FUN_00010636(&PTR_PTR_0001ccd8,&LAB_00010886,param_1);
  if (cVar2 != '\0') {
    FUN_000157a6((void *)(param_1 + 0x1088),0x80);
    cVar2 = FUN_00010816((undefined4 *)(param_1 + 0x12de));
    if (cVar2 == '\0') {
      *(undefined4 *)(param_1 + 0xf04) = 0;
      WRITE_REGISTER_ULONG(*(undefined4 *)(param_1 + 0xee0),0);
    }
  }
  cVar2 = FUN_00010636(&PTR_PTR_0001ccd8,&LAB_0001089a,param_1);
  if (cVar2 != '\0') {
    FUN_000157a6((void *)(param_1 + 0x1088),0x800);
    cVar2 = FUN_00010816((undefined4 *)(param_1 + 0x12de));
    if (cVar2 == '\0') {
      *(undefined4 *)(param_1 + 0xf04) = 0;
      WRITE_REGISTER_ULONG(*(undefined4 *)(param_1 + 0xee0),0);
    }
  }
  cVar2 = FUN_00010636(&PTR_PTR_0001ccd8,&LAB_000108ae,param_1);
  if (cVar2 != '\0') {
    FUN_000157a6((void *)(param_1 + 0x1088),0x100);
    FUN_00010816((undefined4 *)(param_1 + 0x12de));
  }
  cVar2 = FUN_00010636(&PTR_PTR_0001ccd8,&LAB_000108c2,param_1);
  if (cVar2 != '\0') {
    iVar1 = param_1 + 0x10b9;
    bVar3 = FUN_000176a2(iVar1);
    if (bVar3) {
      FUN_000157a6((void *)(param_1 + 0x1088),0);
      FUN_00010816((undefined4 *)(param_1 + 0x12de));
    }
    uVar4 = FUN_000176d0(iVar1);
    if ((char)uVar4 != '\0') {
      KeSetEvent(iVar1,0,0);
    }
  }
  return;
}

