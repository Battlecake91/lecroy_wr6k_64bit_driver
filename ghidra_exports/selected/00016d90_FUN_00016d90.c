
int __thiscall FUN_00016d90(void *this,byte *param_1,uint param_2,undefined4 param_3)

{
  byte bVar1;
  byte bVar2;
  int iVar3;
  byte *pbVar4;
  undefined8 uVar5;
  undefined8 local_20;
  byte local_18;
  undefined3 uStack_17;
  undefined1 local_14;
  undefined3 uStack_13;
  int local_10;
  uint local_c;
  char local_5;
  
  iVar3 = -0x3fffffff;
  local_10 = 3;
  local_5 = '\x01';
  if ((param_1 != (byte *)0x0) && (param_2 != 0)) {
    while( true ) {
      iVar3 = FUN_00016cb0(this);
      if (*(char *)((int)this + 0x18) == '\0') break;
      if (iVar3 == 0) {
        FUN_00016cf0(this,0xcc);
        FUN_00016cf0(this,0xf);
        _local_14 = CONCAT31(uStack_13,(char)((uint)param_3 >> 8));
        iVar3 = FUN_00016d82(this,(byte)param_3);
        if (iVar3 == 0) {
          iVar3 = FUN_00016d82(this,(byte)_local_14);
          if (iVar3 == 0) {
            if (param_2 != 0) {
              local_c = param_2;
              pbVar4 = param_1;
              do {
                FUN_00016d82(this,*pbVar4);
                pbVar4 = pbVar4 + 1;
                local_c = local_c - 1;
              } while (local_c != 0);
            }
            iVar3 = FUN_00016cb0(this);
            if (iVar3 == 0) {
              FUN_00016cf0(this,0xcc);
              FUN_00016cf0(this,0xaa);
              FUN_00016d22(this);
              FUN_00016d22(this);
              bVar1 = FUN_00016d22(this);
              local_c = 0;
              _local_18 = CONCAT31(uStack_17,bVar1);
              pbVar4 = param_1;
              if (param_2 != 0) {
                do {
                  bVar2 = FUN_00016d22(this);
                  bVar1 = *pbVar4;
                  pbVar4 = pbVar4 + 1;
                  if (bVar2 != bVar1) {
                    iVar3 = -0x3fffffff;
                    goto LAB_00016f03;
                  }
                  local_c = local_c + 1;
                } while (local_c < param_2);
              }
              iVar3 = FUN_00016cb0(this);
              if (iVar3 == 0) {
                FUN_00016cf0(this,0xcc);
                FUN_00016cf0(this,0x55);
                iVar3 = FUN_00016d82(this,(byte)param_3);
                if (iVar3 == 0) {
                  iVar3 = FUN_00016d82(this,(byte)_local_14);
                  if (iVar3 == 0) {
                    iVar3 = FUN_00016d82(this,(byte)_local_18);
                    if (iVar3 == 0) {
                      uVar5 = RtlConvertLongToLargeInteger(0xfff0bdc0);
                      local_20 = uVar5;
                      iVar3 = KeDelayExecutionThread(0,0,&local_20);
                      local_5 = '\0';
                    }
                  }
                }
              }
            }
          }
        }
      }
LAB_00016f03:
      if (local_5 == '\0') {
        return iVar3;
      }
      local_10 = local_10 + -1;
      if (local_10 < 1) {
        return iVar3;
      }
    }
    iVar3 = -0x3fffff40;
  }
  return iVar3;
}

