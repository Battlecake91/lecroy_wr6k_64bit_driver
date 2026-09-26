
uint * __thiscall FUN_0001919a(void *this,int param_1)

{
  byte bVar1;
  byte bVar2;
  char *pcVar3;
  
  bVar1 = **(byte **)(param_1 + 0x60);
  bVar2 = (*(byte **)(param_1 + 0x60))[1];
  FUN_00018a1c(this,*(int *)((int)this + 0x1c),"IRP: Major=0x%x");
  if (bVar1 == 0x1b) {
    pcVar3 = " Minor=%s (0x%x) Flags=0x%x\n";
LAB_000191e0:
    FUN_00018a1c(this,*(int *)((int)this + 0x1c),pcVar3);
  }
  else {
    if (bVar1 == 0x16) {
      FUN_00018a1c(this,*(int *)((int)this + 0x1c)," Minor=%s (0x%x) Flags=0x%x  ");
      if ((bVar2 != 3) && (bVar2 != 2)) {
        return this;
      }
      if (*(int *)(*(int *)(param_1 + 0x60) + 8) == 1) {
        FUN_000190ad();
        pcVar3 = "%s\n";
      }
      else {
        FUN_00018fbf();
        pcVar3 = "%s\n";
      }
    }
    else {
      FUN_00018a1c(this,*(int *)((int)this + 0x1c),"  Minor=0x%x Flags=0x%x\n");
      if (bVar1 < 3) {
        return this;
      }
      if (4 < bVar1) {
        if (bVar1 < 0xe) {
          return this;
        }
        if (0xf < bVar1) {
          return this;
        }
        pcVar3 = " Code=0x%x InputSize=0x%x OutputSize=0x%x\n";
        goto LAB_000191e0;
      }
      pcVar3 = " Length=0x%x\n";
    }
    FUN_00018a1c(this,*(int *)((int)this + 0x1c),pcVar3);
  }
  return this;
}

