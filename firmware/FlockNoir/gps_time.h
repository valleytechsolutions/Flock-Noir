#pragma once
inline bool validGpsDate(unsigned year,unsigned month,unsigned day) {
  if(year<2020 || year>2099 || month<1 || month>12 || day<1)return false;
  static const unsigned days[]={31,28,31,30,31,30,31,31,30,31,30,31};
  unsigned limit=days[month-1]+(month==2 && year%4==0);
  return day<=limit;
}
