
undefined1 FUN_0001ba92(char param_1)

{
  undefined1 uVar1;
  
  if (param_1 == '\0') {
    uVar1 = KfAcquireSpinLock();
  }
  else {
    KefAcquireSpinLockAtDpcLevel();
    uVar1 = 2;
  }
  return uVar1;
}

