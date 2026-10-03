
char * FUN_00018fbf(void)

{
  undefined4 in_EAX;
  
  switch(in_EAX) {
  case 0:
    return "PowerSystemUnspecified";
  case 1:
    return "PowerSystemWorking";
  case 2:
    return "PowerSystemSleeping1";
  case 3:
    return "PowerSystemSleeping2";
  case 4:
    return "PowerSystemSleeping3";
  case 5:
    return "PowerSystemHibernate";
  case 6:
    return "PowerSystemShutdown";
  case 7:
    return "PowerSystemMaximum";
  default:
    return "<unknown system power state>";
  }
}

