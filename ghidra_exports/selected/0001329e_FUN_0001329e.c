
undefined4 * __fastcall FUN_0001329e(undefined4 *param_1)

{
  uint *this;
  char cVar1;
  char *pcVar2;
  int iVar3;
  char *pcVar4;
  uint uVar5;
  char *pcVar6;
  undefined4 *puVar7;
  char *pcVar8;
  undefined4 *local_8;
  
  local_8 = param_1;
  FUN_00017a16(param_1);
  this = param_1 + 7;
  FUN_00019362(this,"CKeTraceControl",6,0,5,0,0);
  *(undefined1 *)(param_1 + 0x11) = 1;
  *(undefined1 *)((int)param_1 + 0x45) = 1;
  param_1[0x12] = 0;
  param_1[8] = 2;
  FUN_00018a1c(this,1,"CKeTraceControl()\n");
  FUN_00017a98(param_1,(uint)*(byte *)(param_1 + 0x11),(uint)*(byte *)((int)param_1 + 0x45),0);
  iVar3 = FUN_00013f3c(param_1,this,&local_8);
  if (iVar3 < 0) {
    FUN_00018a1c(this,3,"Failed to add KeTraceControl trace to trace list\n");
  }
  else {
    FUN_00018a1c(this,1,"KeTraceControl trace %s (%p) added successfully at index %d\n");
    iVar3 = FUN_00010380(0x110);
    param_1[0x12] = iVar3;
    if (iVar3 != 0) {
      *(undefined4 *)(iVar3 + 4) = 1;
      *(undefined4 *)param_1[0x12] = 0x110;
      *(undefined4 *)(param_1[0x12] + 0x10c) = 0;
      *(undefined4 *)(param_1[0x12] + 0x108) = param_1[8];
      if ((param_1[10] == 0) || ((undefined4 *)(param_1[0x12] + 8) == (undefined4 *)0x0)) {
        param_1[0xe] = 3;
        FUN_00010750(this);
      }
      else {
        puVar7 = (undefined4 *)(param_1[0x12] + 8);
        for (iVar3 = 0x40; iVar3 != 0; iVar3 = iVar3 + -1) {
          *puVar7 = 0;
          puVar7 = puVar7 + 1;
        }
        pcVar2 = (char *)param_1[10];
        pcVar4 = pcVar2;
        do {
          cVar1 = *pcVar4;
          pcVar4 = pcVar4 + 1;
        } while (cVar1 != '\0');
        pcVar6 = pcVar2;
        pcVar8 = (char *)(param_1[0x12] + 8);
        for (uVar5 = (uint)((int)pcVar4 - (int)(pcVar2 + 1)) >> 2; uVar5 != 0; uVar5 = uVar5 - 1) {
          *(undefined4 *)pcVar8 = *(undefined4 *)pcVar6;
          pcVar6 = pcVar6 + 4;
          pcVar8 = pcVar8 + 4;
        }
        for (uVar5 = (int)pcVar4 - (int)(pcVar2 + 1) & 3; uVar5 != 0; uVar5 = uVar5 - 1) {
          *pcVar8 = *pcVar6;
          pcVar6 = pcVar6 + 1;
          pcVar8 = pcVar8 + 1;
        }
      }
    }
  }
  return param_1;
}

