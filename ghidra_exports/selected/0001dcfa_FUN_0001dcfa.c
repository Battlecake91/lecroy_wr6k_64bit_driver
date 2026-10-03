
void __thiscall FUN_0001dcfa(void *this,undefined4 param_1)

{
  FUN_0001bc5e(this,(int)this + 0xc,'\x01');
  if (*(int *)((int)this + 4) != 0) {
    KeInitializeMutex(*(int *)((int)this + 4),param_1);
  }
  return;
}

