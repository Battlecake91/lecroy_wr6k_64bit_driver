
uint __thiscall FUN_000176e6(void *this,ushort *param_1,uint param_2,int param_3)

{
  uint uVar1;
  undefined4 uVar2;
  uint uVar3;
  int iVar4;
  undefined8 uVar5;
  undefined8 local_18;
  uint local_10;
  ushort *local_c;
  uint local_8;
  
  if ((param_2 & 1) == 0) {
    local_8 = param_2 >> 1;
    param_2 = 0;
    local_c = param_1;
    do {
      if (local_8 < 0x79) {
        uVar1 = param_2 & 0xffffbf00 | local_8 & 0xff;
      }
      else {
        uVar1 = param_2 & 0xffffff78 | 0x4078;
      }
      param_1 = (ushort *)0x0;
      uVar2 = FUN_00017560((int)this);
      if ((char)uVar2 == '\0') {
        do {
          uVar5 = RtlConvertLongToLargeInteger(0xfffe7960);
          local_18 = uVar5;
          uVar3 = KeDelayExecutionThread(0,0,&local_18);
          param_1 = (ushort *)((int)param_1 + 1);
          if (param_3 * 100 < (int)param_1) {
            return uVar3 & 0xffffff00;
          }
          uVar2 = FUN_00017560((int)this);
        } while ((char)uVar2 == '\0');
      }
      param_1 = (ushort *)(uVar1 & 0xff);
      if (param_1 != (ushort *)0x0) {
        iVar4 = 0x420;
        do {
          local_10 = (uint)*local_c;
          WRITE_REGISTER_BUFFER_ULONG
                    (*(int *)(*(int *)((int)this + 0x100) + 0x10) + iVar4,&local_10,1);
          local_c = local_c + 1;
          iVar4 = iVar4 + 4;
          param_1 = (ushort *)((int)param_1 + -1);
        } while (param_1 != (ushort *)0x0);
      }
      FUN_000107fe((void *)((int)this + 0x60),local_8);
      param_2 = uVar1 | 0x8000;
      FUN_000107fe((void *)((int)this + 0x10),uVar1 | 0x8000);
      local_8 = local_8 - (uVar1 & 0xff);
    } while ((uVar1 & 0x4000) != 0);
    uVar1 = 1;
  }
  else {
    uVar1 = param_2 & 0xffffff00;
  }
  return uVar1;
}

