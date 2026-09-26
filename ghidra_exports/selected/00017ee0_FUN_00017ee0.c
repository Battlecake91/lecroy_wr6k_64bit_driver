
uint __thiscall FUN_00017ee0(void *this,uint param_1,char param_2)

{
  uint in_EAX;
  uint uVar1;
  undefined4 uVar2;
  uint local_8;
  
  if ((*(int *)((int)this + 0x18) == 0) && (*(int *)((int)this + 0xc) == 0)) {
    uVar1 = in_EAX & 0xffffff00;
  }
  else {
    local_8 = 0xffffffff;
    FUN_00017eac((void *)((int)this + 8),param_1,&local_8);
    uVar2 = FUN_00013f3c((void *)((int)this + 0x5c),
                         ((uint)(param_2 != '\0') | (local_8 & 0x1ff) << 10) << 6 | param_1 & 0x3f,
                         (undefined4 *)&param_2);
    *(char *)((int)this + 0x78) = *(char *)((int)this + 0x68) + '\x01';
    uVar1 = CONCAT31((int3)((uint)uVar2 >> 8),1);
  }
  return uVar1;
}

