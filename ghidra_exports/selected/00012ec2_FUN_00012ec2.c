
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

uint FUN_00012ec2(int param_1)

{
  uint in_EAX;
  uint uVar1;
  
  if (param_1 == 0) {
    uVar1 = in_EAX & 0xffffff00;
  }
  else {
    _DAT_0001ce10 = _DAT_0001ce10 | DAT_0001ce1c;
    uVar1 = CONCAT31((int3)(DAT_0001ce1c >> 8),1);
  }
  return uVar1;
}

