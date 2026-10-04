
undefined4 FUN_0001be72(undefined4 param_1,int param_2,int *param_3)

{
  byte *pbVar1;
  undefined4 uVar2;
  
  if ((code *)param_3[1] == (code *)0x0) {
    if ((char)param_3[5] == '\0') {
      if (*(char *)(param_2 + 0x21) != '\0') {
        pbVar1 = (byte *)(*(int *)(param_2 + 0x60) + 3);
        *pbVar1 = *pbVar1 | 1;
      }
      uVar2 = 0;
    }
    else {
      uVar2 = 0xc0000016;
    }
  }
  else {
    uVar2 = (*(code *)param_3[1])(param_1,param_2,param_3[2]);
  }
  param_3[4] = *(int *)(param_2 + 0x1c);
  param_3[3] = *(int *)(param_2 + 0x18);
  KeSetEvent(*(undefined4 *)(*param_3 + 4),0,0);
  return uVar2;
}

