
void * __fastcall FUN_00019b82(void *param_1)

{
  FUN_0001bfda(param_1,0);
  *(undefined1 *)((int)param_1 + 0x10) = 0;
  *(undefined4 *)((int)param_1 + 8) = 0x58;
  KeInitializeSpinLock((int)param_1 + 0xc);
  return param_1;
}

