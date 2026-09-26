
void __thiscall FUN_00011b18(void *this,undefined4 param_1,undefined4 param_2)

{
  int iVar1;
  void *pvVar2;
  
  pvVar2 = this;
  iVar1 = ObReferenceObjectByHandle(param_1,2,*(undefined4 *)ExEventObjectType_exref,param_2,this,0)
  ;
  if (iVar1 == 0) {
    *(int *)((int)this + 4) = *(int *)((int)this + 4) + 1;
    *(void **)((int)this + 0xc) = pvVar2;
  }
  return;
}

