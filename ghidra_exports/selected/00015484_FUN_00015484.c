
uint __thiscall FUN_00015484(void *this,int param_1)

{
  *(int *)((int)this + 0x10) = *(int *)((int)this + 0x10) + param_1;
  return -(uint)(*(uint *)((int)this + 0x10) <
                (uint)(*(int *)((int)this + 0xc) + *(int *)((int)this + 8))) &
         *(uint *)((int)this + 0x10);
}

