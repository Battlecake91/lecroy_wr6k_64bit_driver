
undefined4 __thiscall FUN_00012ede(void *this,int param_1)

{
  undefined **ppuVar1;
  undefined4 uVar2;
  
  if ((*(int *)(*(int *)(param_1 + 0x60) + 8) == 4) &&
     (*(undefined4 **)(param_1 + 0xc) != (undefined4 *)0x0)) {
    DAT_0001ce1c = **(undefined4 **)(param_1 + 0xc);
    ppuVar1 = FUN_00010a88();
    (**(code **)*ppuVar1)(&LAB_00012ec2,this);
    (**(code **)(*(int *)this + 0x24))(0,0);
    *(undefined4 *)(param_1 + 0x1c) = 0;
    *(undefined4 *)(param_1 + 0x18) = 0;
    uVar2 = 0;
  }
  else {
    uVar2 = 0xc000000d;
  }
  return uVar2;
}

