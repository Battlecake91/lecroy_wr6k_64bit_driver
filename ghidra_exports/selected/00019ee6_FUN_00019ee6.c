
undefined4 * __fastcall FUN_00019ee6(int param_1)

{
  undefined4 *puVar1;
  undefined4 *puVar2;
  int iVar3;
  undefined4 unaff_EBX;
  undefined4 unaff_ESI;
  
  puVar1 = (undefined4 *)ExAllocatePoolWithTag(1,0x14,0x206d6457);
  if (puVar1 == (undefined4 *)0x0) {
    puVar1 = (undefined4 *)0x0;
  }
  else {
    *puVar1 = unaff_ESI;
    *(undefined2 *)((int)puVar1 + 6) = 0;
    *(undefined2 *)(puVar1 + 1) = 0;
    puVar1[2] = 0;
    puVar1[4] = 0;
    puVar1[3] = 0;
  }
  if (puVar1 == (undefined4 *)0x0) {
    puVar2 = (undefined4 *)0x0;
  }
  else {
    puVar2 = puVar1 + 1;
    iVar3 = IoRegisterDeviceInterface(*(undefined4 *)(param_1 + 0x34),unaff_ESI,unaff_EBX,puVar2);
    if (iVar3 < 0) {
      puVar2 = (undefined4 *)0x0;
    }
    else {
      FUN_00019c08((void *)(param_1 + 0x1d0),(int)puVar1);
    }
  }
  return puVar2;
}

