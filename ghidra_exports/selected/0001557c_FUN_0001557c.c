
void __thiscall FUN_0001557c(void *this,int param_1,char param_2)

{
  undefined8 local_c;
  
  if ((*(char *)((int)this + 9) != '\0') &&
     ((*(char *)((int)this + 8) != '\0' || (param_2 != '\0')))) {
    local_c._0_4_ = this;
    local_c._4_4_ = this;
    local_c = RtlConvertLongToLargeInteger(param_1 * -10000);
    FUN_00015536(this,'\x01');
    KeDelayExecutionThread(0,0,&local_c);
    FUN_00015536(this,'\0');
  }
  return;
}

