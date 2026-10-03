
void __thiscall FUN_0001bb4e(void *this,int param_1,int param_2)

{
  int iVar1;
  char cVar2;
  undefined1 uVar3;
  void *pvVar4;
  void *pvVar5;
  uint unaff_ESI;
  LONG *unaff_EDI;
  void *local_c;
  void *local_8;
  
  local_c = this;
  local_8 = this;
  do {
    uVar3 = FUN_0001baca('\0');
    local_8 = (void *)CONCAT31(local_8._1_3_,uVar3);
    iVar1 = *(int *)(*(int *)this + 4);
    pvVar5 = (void *)FUN_0001bb0e(this,iVar1);
    local_c = pvVar5;
    while( true ) {
      if (pvVar5 == (void *)0x0) goto LAB_0001bbfc;
      if ((param_2 == 0) ||
         ((*(int *)((int)pvVar5 + 0x60) != 0 &&
          (*(int *)(*(int *)((int)pvVar5 + 0x60) + 0x18) == param_2)))) break;
      pvVar5 = (void *)FUN_0001bb2a(this,(int)pvVar5);
      local_c = pvVar5;
      pvVar4 = (void *)FUN_0001bb0e(this,iVar1);
      if (pvVar5 == pvVar4) {
LAB_0001bbfc:
        FUN_0001baec((char)local_8);
        return;
      }
    }
    FUN_0001baec((char)local_8);
    cVar2 = KeRemoveEntryDeviceQueue(*(undefined4 *)this,(int)pvVar5 + 0x40);
    if (cVar2 != '\0') {
      FUN_000193b8();
      InterlockedExchange(unaff_EDI,unaff_ESI);
      unaff_ESI = (uint)DAT_0001d26c;
      unaff_EDI = (LONG *)0x1bbc0;
      IoReleaseCancelSpinLock();
      *(undefined4 *)((int)pvVar5 + 0x1c) = 0;
      FUN_00010798(&local_c,param_1,0xc0000120);
    }
  } while( true );
}

