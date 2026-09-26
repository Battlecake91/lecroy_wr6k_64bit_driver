
void __thiscall FUN_00010636(void *this,undefined *param_1,undefined4 param_2)

{
  if (*(int *)((int)this + 4) == 0) {
    (*(code *)param_1)(param_2);
  }
  else {
    KeSynchronizeExecution(*(int *)((int)this + 4),param_1);
  }
  return;
}

