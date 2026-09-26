
void __thiscall FUN_00015536(void *this,char param_1)

{
  undefined4 *puVar1;
  
  puVar1 = *(undefined4 **)((int)this + 4);
  if (param_1 == '\0') {
    puVar1[9] = 0;
  }
  else {
    puVar1[9] = 1;
  }
  WRITE_REGISTER_ULONG(*puVar1,param_1 != '\0');
  return;
}

