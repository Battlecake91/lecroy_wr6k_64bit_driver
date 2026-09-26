
undefined4 __thiscall FUN_00017578(void *this,undefined2 *param_1,uint param_2,int *param_3)

{
  undefined2 *puVar1;
  uint uVar2;
  uint uVar3;
  int iVar4;
  undefined8 uVar5;
  undefined8 local_1c;
  int local_14;
  uint local_10;
  undefined4 local_c;
  uint local_8;
  
  if ((param_2 & 1) == 0) {
    *param_3 = 0;
    local_c = 0;
    do {
      uVar2 = READ_REGISTER_ULONG(*(undefined4 *)((int)this + 0x38));
      if (((char)(uVar2 >> 8) < '\0') && (uVar3 = uVar2 & 0xff, uVar3 != 0)) {
        if ((uVar3 < 0x79) && (local_14 = uVar3 * 2, (uint)(*param_3 + local_14) <= param_2)) {
          if (uVar3 != 0) {
            iVar4 = 0x600;
            local_10 = uVar3;
            do {
              local_8 = 0;
              READ_REGISTER_BUFFER_ULONG
                        (*(int *)(*(int *)((int)this + 0x100) + 0x10) + iVar4,&local_8,1);
              puVar1 = param_1 + 1;
              iVar4 = iVar4 + 4;
              local_10 = local_10 - 1;
              *param_1 = (undefined2)local_8;
              param_1 = puVar1;
            } while (local_10 != 0);
            local_10 = 0;
          }
          *param_3 = *param_3 + local_14;
        }
        else {
          local_c = 0xc000000d;
        }
        KeResetEvent(this);
        local_8 = uVar2 & 0xffff7f00;
        FUN_000107fe((void *)((int)this + 0x38),uVar2 & 0x7f00);
        uVar2 = local_8;
      }
      local_8 = uVar2 >> 0xe & 1;
      if (local_8 != 0) {
        uVar5 = RtlConvertLongToLargeInteger(0xfd050f80);
        local_1c = uVar5;
        iVar4 = KeWaitForSingleObject(this,0,0,0,&local_1c);
        if ((iVar4 != 0) && (iVar4 == 0x102)) {
          return 0x102;
        }
      }
    } while (local_8 == 1);
  }
  else {
    local_c = 0xc000000d;
  }
  return local_c;
}

