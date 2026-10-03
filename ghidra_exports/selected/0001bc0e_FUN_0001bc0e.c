
bool __thiscall FUN_0001bc0e(void *this,int param_1,undefined4 param_2,int *param_3)

{
  int iVar1;
  undefined1 uVar2;
  LONG unaff_EBX;
  LONG *unaff_ESI;
  bool bVar3;
  
  bVar3 = false;
  uVar2 = FUN_000193b8();
  iVar1 = *(int *)this;
  if (((param_1 == 0) || ((iVar1 == *param_3 && (*(int *)(iVar1 + 0x38) == param_1)))) &&
     (bVar3 = *(char *)(iVar1 + 0x24) == '\0', bVar3)) {
    InterlockedExchange(unaff_ESI,unaff_EBX);
  }
  IoReleaseCancelSpinLock(uVar2);
  return bVar3;
}

