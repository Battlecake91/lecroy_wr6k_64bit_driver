
void __fastcall FUN_0001888e(int *param_1)

{
  int iVar1;
  undefined4 uVar2;
  int iVar3;
  int *piVar4;
  int local_2c;
  undefined4 local_28;
  undefined1 local_c [8];
  
  piVar4 = param_1 + 1;
  for (iVar3 = 8; iVar3 != 0; iVar3 = iVar3 + -1) {
    *piVar4 = 0;
    piVar4 = piVar4 + 1;
  }
  if (*param_1 != 0) {
    uVar2 = IoGetAttachedDeviceReference(*param_1);
    FUN_0001dd1e(&local_2c,0,0);
    iVar3 = IoBuildSynchronousFsdRequest(0x1b,uVar2,0,0,0,local_28,local_c);
    if (iVar3 != 0) {
      iVar1 = *(int *)(iVar3 + 0x60);
      *(undefined1 *)(iVar1 + -0x23) = 8;
      *(undefined **)(iVar1 + -0x20) = &DAT_0001cbc8;
      *(undefined2 *)(iVar1 + -0x1c) = 0x20;
      *(undefined2 *)(iVar1 + -0x1a) = 1;
      *(int **)(iVar1 + -0x18) = param_1 + 1;
      *(undefined4 *)(iVar1 + -0x14) = 0;
      *(undefined4 *)(iVar3 + 0x18) = 0xc00000bb;
      iVar3 = IofCallDriver();
      if (iVar3 == 0x103) {
        KeWaitForSingleObject(local_28,0,0,0,0);
      }
    }
    ObfDereferenceObject();
    FUN_00018870(&local_2c);
  }
  return;
}

