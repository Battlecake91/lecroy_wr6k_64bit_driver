
char * FUN_000190ad(void)

{
  int in_EAX;
  
  if (in_EAX == 0) {
    return "PowerDeviceUnspecified";
  }
  if (in_EAX == 1) {
    return "PowerDeviceD0";
  }
  if (in_EAX == 2) {
    return "PowerDeviceD1";
  }
  if (in_EAX != 3) {
    if (in_EAX == 4) {
      return "PowerDeviceD3";
    }
    if (in_EAX != 5) {
      return "<unknown device power state>";
    }
    return "PowerDeviceMaximum";
  }
  return "PowerDeviceD2";
}

