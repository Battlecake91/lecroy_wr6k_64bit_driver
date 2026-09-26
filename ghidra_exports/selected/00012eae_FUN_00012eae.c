
uint FUN_00012eae(int param_1)

{
  uint in_EAX;
  uint uVar1;
  
  if (param_1 == 0) {
    uVar1 = in_EAX & 0xffffff00;
  }
  else {
    uVar1 = FUN_00011e46();
  }
  return uVar1;
}

