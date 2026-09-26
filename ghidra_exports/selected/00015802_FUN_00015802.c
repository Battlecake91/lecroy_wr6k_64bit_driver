
char __thiscall FUN_00015802(void *this,byte param_1,char param_2)

{
  if (*(char *)this != '\0') {
    FUN_000107fe(*(void **)((int)this + 4),(uint)(param_2 != '\0') << 8 | param_1 & 0xf);
  }
  return *(char *)this;
}

