
int FUN_00015720(int param_1)

{
  short sVar1;
  uint3 uVar3;
  int iVar2;
  
  sVar1 = *(short *)(param_1 + 4);
  uVar3 = (uint3)(CONCAT22((short)((uint)param_1 >> 0x10),sVar1) >> 8);
  if ((sVar1 == 1) || (sVar1 == 2)) {
    iVar2 = CONCAT31(uVar3,1);
  }
  else {
    iVar2 = (uint)uVar3 << 8;
  }
  return iVar2;
}

