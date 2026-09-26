
bool __thiscall FUN_00017c16(void *this,uint param_1,uint param_2,uint param_3)

{
  char cVar1;
  uint uVar2;
  
  cVar1 = *(char *)this;
  if (cVar1 != '\0') {
    FUN_000179e2(*(void **)((int)this + 4),param_1 >> 0x10);
    uVar2 = param_1 >> 0x10 | 0x10000;
    FUN_000179e2(*(void **)((int)this + 4),uVar2 ^ (param_2 ^ uVar2) & 0xffff);
    FUN_000179e2(*(void **)((int)this + 4),param_2 >> 0x10 | 0x20000);
    uVar2 = param_2 >> 0x10 | 0x30000;
    FUN_000179e2(*(void **)((int)this + 4),uVar2 ^ (param_3 ^ uVar2) & 0xffff);
    FUN_000179e2(*(void **)((int)this + 4),param_3 >> 0x10 | 0x40000);
    FUN_000107fe(*(void **)((int)this + 8),0x105);
  }
  return cVar1 != '\0';
}

