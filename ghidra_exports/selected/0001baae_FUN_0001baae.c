
void FUN_0001baae(char param_1)

{
  if (param_1 == '\x02') {
    KefReleaseSpinLockFromDpcLevel();
  }
  else {
    KfReleaseSpinLock();
  }
  return;
}

