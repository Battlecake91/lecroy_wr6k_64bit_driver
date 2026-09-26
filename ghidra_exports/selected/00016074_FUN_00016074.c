
void __thiscall FUN_00016074(void *this,char param_1)

{
  undefined **ppuVar1;
  
  if (param_1 == '\0') {
    DAT_0001ce18 = DAT_0001ce18 & 0xfffffffb;
  }
  else {
    DAT_0001ce18 = DAT_0001ce18 | 4;
  }
  ppuVar1 = FUN_00010a88();
  (**(code **)*ppuVar1)(FUN_00012eae,*(undefined4 *)((int)this + 0x19e));
  return;
}

