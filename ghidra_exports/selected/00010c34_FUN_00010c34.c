
void __fastcall FUN_00010c34(int param_1)

{
  int unaff_ESI;
  
  IoStartNextPacket(*(undefined4 *)(param_1 + -0x1dc),1);
  if (unaff_ESI != 0) {
    IofCompleteRequest();
    FUN_0001955a(param_1 + -0x1e0);
  }
  return;
}

