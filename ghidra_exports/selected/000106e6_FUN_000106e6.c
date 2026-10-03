
void __thiscall FUN_000106e6(void *this,uint *param_1,int param_2,undefined4 param_3)

{
  int iVar1;
  int *piVar2;
  undefined4 uVar3;
  void *local_c;
  void *pvStack_8;
  
  local_c = this;
  pvStack_8 = this;
  FUN_0001dc76(&param_2,param_1,'\x03',param_2);
  if (param_2 == 0) {
    uVar3 = 0xc000008a;
  }
  else {
    iVar1 = *(int *)(param_2 + 0xc);
    piVar2 = (int *)FUN_00010656(&param_2,&local_c);
    uVar3 = FUN_000106cc(this,*piVar2,piVar2[1],iVar1,(char)param_3);
  }
  *(undefined4 *)this = uVar3;
  return;
}

