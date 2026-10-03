
byte __fastcall FUN_00016d22(void *param_1)

{
  byte bVar1;
  byte bVar2;
  int iVar3;
  byte bVar4;
  
  bVar2 = 0;
  bVar4 = 0;
  iVar3 = 8;
  do {
    FUN_00016c92(param_1,3);
    do {
      bVar1 = FUN_00016ca4((int)param_1);
    } while ((bVar1 & 1) != 0);
    bVar1 = FUN_00016ca4((int)param_1);
    bVar2 = bVar2 | (bVar1 >> 1 & 1) << (bVar4 & 0x1f);
    bVar4 = bVar4 + 1;
    iVar3 = iVar3 + -1;
  } while (iVar3 != 0);
  return bVar2;
}

