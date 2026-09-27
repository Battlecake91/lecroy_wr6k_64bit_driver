
void __thiscall FUN_000123b4(void *this,undefined4 *param_1,int param_2)

{
  char cVar1;
  int iVar2;
  undefined4 uVar3;
  undefined4 *puVar4;
  undefined4 *puVar5;
  char *pcVar6;
  int local_8;
  
  FUN_00018a1c((uint *)((int)this + 0x38),1,"UpdateAndGetList() \n");
  if (param_1 == (undefined4 *)0x0) {
    pcVar6 = "UpdateAndGetList: Null parameter\n";
  }
  else {
    iVar2 = FUN_00012386((int)this);
    if (param_2 == iVar2) {
      param_2 = 0;
      if (*(int *)((int)this + 0x28) == -1 || *(int *)((int)this + 0x28) + 1 < 0) {
        return;
      }
      local_8 = 0;
      do {
        puVar4 = *(undefined4 **)(*(int *)((int)this + 0x10) + param_2 * 4);
        cVar1 = *(char *)(puVar4 + 8);
        if (cVar1 == '\x01') {
          uVar3 = *puVar4;
LAB_00012449:
          uVar3 = READ_REGISTER_ULONG(uVar3);
LAB_0001244f:
          *(undefined4 *)(local_8 + 0x106 + *(int *)((int)this + 0x2c)) = uVar3;
        }
        else {
          if (cVar1 == '\x02') {
            uVar3 = puVar4[9];
            goto LAB_0001244f;
          }
          uVar3 = *puVar4;
          if (cVar1 == '\x04') goto LAB_00012449;
          iVar2 = *(int *)((int)this + 0x2c);
          uVar3 = READ_REGISTER_ULONG(uVar3);
          *(undefined4 *)(iVar2 + local_8 + 0x106) = uVar3;
        }
        puVar4 = (undefined4 *)(*(int *)((int)this + 0x2c) + local_8);
        puVar5 = param_1;
        for (iVar2 = 0x42; iVar2 != 0; iVar2 = iVar2 + -1) {
          *puVar5 = *puVar4;
          puVar4 = puVar4 + 1;
          puVar5 = puVar5 + 1;
        }
        *(undefined2 *)puVar5 = *(undefined2 *)puVar4;
        FUN_00018a1c((uint *)((int)this + 0x38),1,
                     "iteration[0x%2d]: %s, BAR: %d, offset 0x%x, type %d, data: 0x%x\n");
        local_8 = local_8 + 0x10a;
        param_1 = (undefined4 *)((int)param_1 + 0x10a);
        param_2 = param_2 + 1;
        if (*(int *)((int)this + 0x28) + 1 <= param_2) {
          return;
        }
      } while( true );
    }
    pcVar6 = "UpdateAndGetList: invalid buffer size\n";
  }
  FUN_00018a1c((uint *)((int)this + 0x38),3,pcVar6);
  return;
}

