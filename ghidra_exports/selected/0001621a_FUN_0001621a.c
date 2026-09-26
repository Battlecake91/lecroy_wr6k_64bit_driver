
undefined2 __thiscall FUN_0001621a(void *this,char *param_1)

{
  uint *puVar1;
  char cVar2;
  int iVar3;
  undefined2 uVar4;
  undefined8 uVar5;
  undefined4 local_14;
  undefined4 local_10;
  uint local_c;
  int local_8;
  
  if (*param_1 == '\x01') {
    uVar4 = 0;
    local_14 = 0;
    local_10 = 0;
    iVar3 = KeWaitForSingleObject
                      (*(undefined4 *)(*(int *)((int)this + 0x186) + 0xc),0,0,1,&local_14);
    if (iVar3 == 0) {
      iVar3 = *(int *)(param_1 + 1);
      if (iVar3 == 0) {
        iVar3 = 1;
      }
      KeQuerySystemTime((int)this + 0x18a);
      cVar2 = (**(code **)**(undefined4 **)((int)this + 0x186))
                        (iVar3 * -10000,iVar3 * -10000 >> 0x1f);
      if (cVar2 == '\0') {
        *(int *)((int)this + 0x192) = iVar3;
        goto LAB_0001633b;
      }
    }
    else if (iVar3 == 0x102) {
      local_c = 0;
      local_8 = 0;
      KeQuerySystemTime(&local_c);
      param_1 = *(char **)(param_1 + 1);
      puVar1 = (uint *)((int)this + 0x18a);
      uVar5 = __alldiv(local_c - *puVar1,
                       (local_8 - *(int *)((int)this + 0x18e)) - (uint)(local_c < *puVar1),10000,0);
      if ((char *)(*(int *)((int)this + 0x192) - (int)uVar5) < param_1) {
        if (param_1 == (char *)0x0) {
          param_1 = (char *)0x1;
        }
        *(char **)((int)this + 0x192) = param_1;
        KeQuerySystemTime(puVar1);
        (**(code **)**(undefined4 **)((int)this + 0x186))
                  ((int)param_1 * -10000,(int)param_1 * -10000 >> 0x1f);
      }
      uVar4 = 0;
      goto LAB_0001633b;
    }
    uVar4 = 8;
  }
  else {
    uVar4 = 4;
  }
LAB_0001633b:
  FUN_00015a88(this,uVar4);
  return uVar4;
}

