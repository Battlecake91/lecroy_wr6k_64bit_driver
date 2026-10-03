
undefined4 * __thiscall FUN_000104a4(void *this,undefined4 param_1,undefined4 param_2)

{
  undefined4 uVar1;
  undefined4 *unaff_EDI;
  
  uVar1 = PoSetPowerState(*(undefined4 *)((int)this + 4),1,param_2);
  *(undefined4 *)((int)this + 0x1a4) = param_2;
  *unaff_EDI = uVar1;
  return unaff_EDI;
}

