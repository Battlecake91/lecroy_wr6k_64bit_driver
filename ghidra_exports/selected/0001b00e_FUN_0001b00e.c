
undefined4 __thiscall FUN_0001b00e(int param_1,int param_2)

{
  int iVar1;
  char cVar2;
  undefined4 uVar3;
  LONG unaff_ESI;
  LONG *unaff_EDI;
  
  FUN_000193b8();
  cVar2 = FUN_0001941a(&param_2,'\0');
  iVar1 = param_2;
  if (cVar2 == '\0') {
    FUN_00019fe0((void *)(param_1 + 0xac),param_2);
    InterlockedExchange(unaff_EDI,unaff_ESI);
    IoReleaseCancelSpinLock(DAT_0001d26c);
    uVar3 = 0x103;
  }
  else {
    if (*(int *)(param_2 + 0x38) == 0) {
      *(undefined4 *)(param_2 + 0x1c) = 0;
      IoReleaseCancelSpinLock(DAT_0001d26c);
      *(undefined4 *)(iVar1 + 0x18) = 0xc0000120;
      IofCompleteRequest();
    }
    else {
      IoReleaseCancelSpinLock(DAT_0001d26c);
    }
    uVar3 = 0xc0000120;
  }
  return uVar3;
}

