
int * __thiscall FUN_0001731c(void *this,int *param_1,void *param_2,undefined1 param_3)

{
  uint uVar1;
  int iVar2;
  int *piVar3;
  int *piVar4;
  int iVar5;
  
  if ((((param_1 == (int *)0x0) || (*param_1 == 0)) || (uVar1 = param_1[1], uVar1 == 0)) ||
     (0x6000004 < uVar1)) {
    piVar4 = (int *)0xc000000d;
  }
  else {
    param_1[1] = uVar1 - 4;
    piVar3 = (int *)ExAllocatePoolWithTag(0,0x40,0x206d6457);
    if (piVar3 == (int *)0x0) {
      FUN_00017fd6();
      piVar4 = (int *)0xc000009a;
    }
    else {
      piVar4 = piVar3;
      for (iVar5 = 0x10; iVar5 != 0; iVar5 = iVar5 + -1) {
        *piVar4 = 0;
        piVar4 = piVar4 + 1;
      }
      *piVar3 = *param_1;
      piVar3[1] = param_1[1];
      *(undefined1 *)(piVar3 + 0xe) = 0;
      piVar4 = (int *)FUN_0001807a();
      if (piVar4 == (int *)0x0) {
        *(undefined1 *)(piVar3 + 0xe) = param_3;
        piVar4 = (int *)FUN_00017f8c();
        if (piVar4 == (int *)0x0) {
          iVar5 = FUN_00018194((void *)((int)this + 0x100),piVar3[4],piVar3[5],
                               (undefined4 *)piVar3[2]);
          piVar3[6] = iVar5;
          iVar5 = *(int *)((int)this + 0x104);
          if (iVar5 == 0) {
            *(int *)((int)this + 0x104) = (int)piVar3;
          }
          else {
            do {
              iVar2 = *(int *)(iVar5 + 0x3c);
              if (iVar2 == 0) break;
              iVar5 = iVar2;
            } while (iVar2 != 0);
            *(int **)(iVar5 + 0x3c) = piVar3;
          }
          KeInitializeEvent(piVar3 + 9,0,0);
          iVar5 = IoGetCurrentProcess();
          piVar3[0xd] = iVar5;
          if ((char)piVar3[0xe] == '\0') {
            return piVar3;
          }
          FUN_0001232a(param_2,piVar3[0xd],1);
          return piVar3;
        }
      }
      FUN_00017fd6();
    }
  }
  return piVar4;
}

