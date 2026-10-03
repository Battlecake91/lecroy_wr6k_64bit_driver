
undefined4 FUN_000196b4(int *param_1,int param_2)

{
  byte *pbVar1;
  int *this;
  int iVar2;
  
  iVar2 = param_2;
  this = param_1;
  (**(code **)(*param_1 + 0x114))(param_2);
  if (*(char *)(iVar2 + 0x21) != '\0') {
    pbVar1 = (byte *)(*(int *)(iVar2 + 0x60) + 3);
    *pbVar1 = *pbVar1 | 1;
  }
  FUN_000104a4(this,&param_1,*(undefined4 *)(*(int *)(iVar2 + 0x60) + 0xc));
  this[0x43] = this[0x43] & 0xfffffff9;
  if (((*(byte *)(this + 0x4a) & 0x20) != 0) && (*(int *)(*(int *)(iVar2 + 0x60) + 0xc) == 1)) {
    (**(code **)(*this + 0x108))(0);
  }
  if ((*(byte *)(this + 0x44) & 2) != 0) {
    this[0x43] = this[0x43] | 0x20;
  }
  PoStartNextPowerIrp(iVar2);
  FUN_0001955a((int)this);
  return 0;
}

