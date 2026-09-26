
void __thiscall FUN_000169b4(void *this,ushort *param_1,undefined4 *param_2)

{
  char cVar1;
  ushort *puVar2;
  uint uVar3;
  undefined2 uVar4;
  undefined4 local_10;
  undefined4 *local_8;
  
  puVar2 = param_1;
  if ((char)param_1[4] == '@') {
    cVar1 = *(char *)((int)param_1 + 9);
    if (cVar1 == '\0') {
      param_1 = (ushort *)(uint)*param_1;
      local_8 = (undefined4 *)0x0;
      uVar3 = FUN_000167f4(this,(int *)&local_8,(ushort *)&param_1);
      if (((char)uVar3 == '\0') || ((ushort)param_1 == 0)) {
        *(undefined2 *)(param_2 + 1) = 0;
      }
      else {
        if (*puVar2 < (ushort)param_1) {
          param_1 = (ushort *)(uint)*puVar2;
        }
        for (uVar3 = ((uint)param_1 & 0xffff) >> 2; uVar3 != 0; uVar3 = uVar3 - 1) {
          *param_2 = *local_8;
          local_8 = local_8 + 1;
          param_2 = param_2 + 1;
        }
        for (uVar3 = (uint)param_1 & 3; uVar3 != 0; uVar3 = uVar3 - 1) {
          *(undefined1 *)param_2 = *(undefined1 *)local_8;
          local_8 = (undefined4 *)((int)local_8 + 1);
          param_2 = (undefined4 *)((int)param_2 + 1);
        }
      }
    }
    else if (cVar1 == '\x01') {
      uVar4 = *(undefined2 *)((int)this + 9);
      local_10 = CONCAT22(*(undefined2 *)((int)this + 0xb),4);
      *param_2 = 0;
      param_2[1] = local_10;
      *(undefined2 *)(param_2 + 2) = uVar4;
    }
    else {
      if (cVar1 == '\x02') {
        uVar4 = 0x10;
      }
      else {
        uVar4 = 2;
      }
      FUN_00015a08((int)param_1,param_2,uVar4);
    }
  }
  return;
}

