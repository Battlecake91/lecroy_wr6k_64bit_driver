
void __thiscall FUN_00016bba(void *this,int param_1,int *param_2)

{
  char cVar1;
  uint uVar2;
  undefined2 uVar3;
  
  if (*(char *)(param_1 + 8) == '@') {
    cVar1 = *(char *)(param_1 + 9);
    if (cVar1 == '\0') {
      uVar2 = FUN_00016a66(this,param_1);
      uVar3 = (undefined2)uVar2;
    }
    else {
      if (cVar1 == '\x01') {
        uVar2 = FUN_000165a6(this,param_1,param_2);
        FUN_00015a08(param_1,param_2,(short)uVar2);
        return;
      }
      if (cVar1 == '\x02') {
        uVar2 = FUN_000166a8(this,param_1);
        uVar3 = (undefined2)uVar2;
      }
      else {
        uVar3 = 2;
      }
    }
    FUN_00015a08(param_1,param_2,uVar3);
  }
  return;
}

