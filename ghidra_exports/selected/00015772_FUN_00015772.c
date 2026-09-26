
void __thiscall FUN_00015772(void *this,char param_1)

{
  undefined **ppuVar1;
  
  if (param_1 == '\0') {
    DAT_0001ce18 = DAT_0001ce18 & 0xffffffdf;
  }
  else {
    DAT_0001ce18 = DAT_0001ce18 | 0x20;
  }
  ppuVar1 = FUN_00010a88();
  (**(code **)*ppuVar1)(FUN_00012eae,*(undefined4 *)((int)this + 0x19e));
  return;
}

