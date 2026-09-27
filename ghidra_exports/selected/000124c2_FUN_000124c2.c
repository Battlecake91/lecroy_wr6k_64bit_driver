
void __thiscall FUN_000124c2(void *this,int param_1,undefined4 *param_2)

{
  uint *puVar1;
  char cVar2;
  undefined4 uVar3;
  int iVar4;
  undefined4 *puVar5;
  
  puVar1 = (uint *)((int)this + 0x38);
  FUN_00018a1c(puVar1,1,"UpdateAndGetList() \n");
  if (param_2 == (undefined4 *)0x0) {
    FUN_00018a1c(puVar1,3,"UpdateAndGetList: Null parameter\n");
    return;
  }
  puVar5 = *(undefined4 **)(*(int *)((int)this + 0x10) + param_1 * 4);
  cVar2 = *(char *)(puVar5 + 8);
  if (cVar2 == '\x01') {
    uVar3 = *puVar5;
LAB_0001253c:
    uVar3 = READ_REGISTER_ULONG(uVar3);
  }
  else {
    if (cVar2 != '\x02') {
      uVar3 = *puVar5;
      if (cVar2 != '\x04') {
        iVar4 = *(int *)((int)this + 0x2c);
        uVar3 = READ_REGISTER_ULONG(uVar3);
        *(undefined4 *)(iVar4 + param_1 * 0x10a + 0x106) = uVar3;
        goto LAB_00012552;
      }
      goto LAB_0001253c;
    }
    uVar3 = puVar5[9];
  }
  *(undefined4 *)(*(int *)((int)this + 0x2c) + 0x106 + param_1 * 0x10a) = uVar3;
LAB_00012552:
  puVar5 = (undefined4 *)(*(int *)((int)this + 0x2c) + param_1 * 0x10a);
  for (iVar4 = 0x42; iVar4 != 0; iVar4 = iVar4 + -1) {
    *param_2 = *puVar5;
    puVar5 = puVar5 + 1;
    param_2 = param_2 + 1;
  }
  *(undefined2 *)param_2 = *(undefined2 *)puVar5;
  FUN_00018a1c(puVar1,1,"index[0x%2d]: %s, BAR: %d, offset 0x%x, type %d, data: 0x%x\n");
  return;
}

