#ifndef TIME_H
#define TIME_H
#include <syscall.h>
#include <stdint.h>
#include <mmu.h>

#define RTC 0x15000000
#define UTC_BASE_YEAR 1970
#define MONTH_PER_YEAR 12
#define DAY_PER_YEAR 365
#define SEC_PER_DAY 86400
#define SEC_PER_HOUR 3600
#define SEC_PER_MIN 60

typedef struct
{
    unsigned short nYear;
    unsigned char nMonth;
    unsigned char nDay;
    unsigned char nHour;
    unsigned char nMin;
    unsigned char nSec;
    unsigned char DayIndex; /* 0 = Sunday */
} mytime_struct;

u_int get_time(u_int *us);
unsigned int mytime_2_utc_sec(mytime_struct *currTime, int daylightSaving);
void utc_sec_2_mytime(unsigned int utc_sec, mytime_struct *result, int daylightSaving);
unsigned char applib_dt_dayindex(unsigned short year, unsigned char month, unsigned char day);
unsigned char applib_dt_last_day_of_mon(unsigned char month, unsigned short year);
unsigned char applib_dt_is_leap_year(unsigned short year);

#endif