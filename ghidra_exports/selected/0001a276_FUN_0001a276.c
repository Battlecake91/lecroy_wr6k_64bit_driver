
void __thiscall FUN_0001a276(void *this,int param_1)

{
  bool bVar1;
  int iVar2;
  int iVar3;
  int *this_00;
  LONG unaff_ESI;
  LONG *unaff_EDI;
  int local_2c [5];
  int local_18 [5];
  
  FUN_00019b82(local_2c);
  FUN_00019b82(local_18);
  while (iVar3 = FUN_0001a022((int *)((int)this + 0xac)), iVar3 != 0) {
    FUN_000193b8();
    if (*(char *)(iVar3 + 0x24) == '\0') {
      if (*(int *)(iVar3 + 0x60) == 0) {
        iVar2 = 0;
      }
      else {
        iVar2 = *(int *)(*(int *)(iVar3 + 0x60) + 0x18);
      }
      if (iVar2 == param_1) {
        InterlockedExchange(unaff_EDI,unaff_ESI);
        this_00 = local_2c;
      }
      else {
        this_00 = local_18;
      }
      FUN_00019fe0(this_00,iVar3);
    }
    IoReleaseCancelSpinLock(DAT_0001d26c);
  }
  while (bVar1 = FUN_0001a040(local_18), !bVar1) {
    iVar3 = FUN_0001a022(local_18);
    FUN_00019fe0((int *)((int)this + 0xac),iVar3);
  }
  bVar1 = FUN_0001a040(local_2c);
  while (bVar1 == false) {
    iVar3 = FUN_0001a022(local_2c);
    *(undefined4 *)(iVar3 + 0x1c) = 0;
    *(undefined4 *)(iVar3 + 0x18) = 0xc0000120;
    IofCompleteRequest();
    bVar1 = FUN_0001a040(local_2c);
  }
  return;
}

