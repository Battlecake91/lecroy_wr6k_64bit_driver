
void __thiscall FUN_00016cf0(void *this,byte param_1)

{
  byte bVar1;
  int iVar2;
  
  iVar2 = 8;
  do {
    if ((param_1 & 1) == 0) {
      bVar1 = 1;
    }
    else {
      bVar1 = 2;
    }
    FUN_00016c92(this,bVar1);
    do {
      bVar1 = FUN_00016ca4((int)this);
    } while ((bVar1 & 1) != 0);
    param_1 = param_1 >> 1;
    iVar2 = iVar2 + -1;
  } while (iVar2 != 0);
  return;
}

