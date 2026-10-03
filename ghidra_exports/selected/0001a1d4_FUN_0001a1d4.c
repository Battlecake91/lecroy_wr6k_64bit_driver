
void __thiscall FUN_0001a1d4(void *this,char param_1)

{
  char cVar1;
  void *pvVar2;
  uint unaff_ESI;
  LONG *unaff_EDI;
  void *local_c;
  int *local_8;
  
  local_c = this;
  local_8 = this;
  do {
    while( true ) {
      FUN_000193b8();
      pvVar2 = (void *)FUN_0001a022(local_8 + 0x2b);
      local_c = pvVar2;
      if (pvVar2 == (void *)0x0) {
        IoReleaseCancelSpinLock(DAT_0001d26c);
        return;
      }
      cVar1 = FUN_0001941a(&local_c,'\0');
      if (cVar1 == '\0') break;
      IoReleaseCancelSpinLock(DAT_0001d26c);
      *(undefined4 *)((int)pvVar2 + 0x1c) = 0;
      *(undefined4 *)((int)pvVar2 + 0x18) = 0xc0000120;
LAB_0001a252:
      IofCompleteRequest();
    }
    InterlockedExchange(unaff_EDI,unaff_ESI);
    unaff_ESI = (uint)DAT_0001d26c;
    unaff_EDI = (LONG *)0x1a241;
    IoReleaseCancelSpinLock();
    if (param_1 != '\0') {
      *(undefined4 *)((int)pvVar2 + 0x1c) = 0;
      *(undefined4 *)((int)pvVar2 + 0x18) = 0xc0000056;
      goto LAB_0001a252;
    }
    (**(code **)(*local_8 + 8))(pvVar2);
  } while( true );
}

