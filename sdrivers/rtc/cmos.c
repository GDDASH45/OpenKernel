#include <stdint.h>
#include <kernel/ports.h>
#include <kernel/rtc.h>

#define CMOS_ADDRESS  0x70
#define CMOS_DATA     0x71

#define CMOS_SECONDS  0x00
#define CMOS_MINUTES  0x02
#define CMOS_HOURS    0x04
#define CMOS_DAY      0x07
#define CMOS_MONTH    0x08
#define CMOS_YEAR     0x09
#define CMOS_STATUS_A 0x0A
#define CMOS_STATUS_B 0x0B
#define CMOS_CENTURY  0x32

#define CMOS_UIP          0x80
#define CMOS_BINARY_MODE  0x04
#define CMOS_24_HOUR_MODE 0x02

#define RTC_READ_ATTEMPTS 8
#define RTC_DEFAULT_CENTURY 20

static uint8_t cmos_read(uint8_t reg)
{
    port_byte_out(CMOS_ADDRESS, reg);
    return port_byte_in(CMOS_DATA);
}

static int bcd_is_valid(uint8_t value)
{
    return (value & 0x0F) <= 9 && ((value >> 4) & 0x0F) <= 9;
}

static uint8_t bcd_to_binary(uint8_t value)
{
    return (uint8_t)((value & 0x0F) + ((value >> 4) * 10));
}

static int rtc_wait_update(void)
{
    uint32_t timeout = 1000000;

    while ((cmos_read(CMOS_STATUS_A) & CMOS_UIP) && timeout != 0) {
        timeout--;
    }

    return timeout != 0;
}

static uint16_t rtc_expand_year(uint8_t year, uint8_t century)
{
    /*
     * QEMU and many PC-compatible RTCs expose the century at CMOS
     * register 0x32. Some firmware leaves it unset or returns 0xFF.
     * In that case retain the conventional 2000-based fallback.
     */
    if (century >= 19 && century <= 99) {
        return (uint16_t)((uint16_t)century * 100u + year);
    }

    return (uint16_t)(2000u + year);
}

static int rtc_date_is_valid(const struct rtc_time *time)
{
    static const uint8_t days_per_month[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };
    uint8_t max_day;

    if (time->year < 1 ||
        time->month < 1 || time->month > 12 ||
        time->hour > 23 || time->minute > 59 ||
        time->second > 59) {
        return 0;
    }

    max_day = days_per_month[time->month - 1];

    if (time->month == 2 &&
        ((time->year % 400u) == 0u ||
         ((time->year % 4u) == 0u &&
          (time->year % 100u) != 0u))) {
        max_day = 29;
    }

    return time->day >= 1 && time->day <= max_day;
}

static int rtc_read_raw(struct rtc_time *time)
{
    uint8_t status_b;
    uint8_t hour;
    uint8_t second;
    uint8_t minute;
    uint8_t day;
    uint8_t month;
    uint8_t year;
    uint8_t century;

    if (!rtc_wait_update()) {
        return -1;
    }

    status_b = cmos_read(CMOS_STATUS_B);
    second = cmos_read(CMOS_SECONDS);
    minute = cmos_read(CMOS_MINUTES);
    hour = cmos_read(CMOS_HOURS);
    day = cmos_read(CMOS_DAY);
    month = cmos_read(CMOS_MONTH);
    year = cmos_read(CMOS_YEAR);
    century = cmos_read(CMOS_CENTURY);

    /*
     * Reject a sample if an RTC update started while its registers
     * were being read. This keeps fields from adjacent seconds apart.
     */
    if (cmos_read(CMOS_STATUS_A) & CMOS_UIP) {
        return -1;
    }

    if (!(status_b & CMOS_BINARY_MODE)) {
        if (!bcd_is_valid(second) ||
            !bcd_is_valid(minute) ||
            !bcd_is_valid((uint8_t)(hour & 0x7F)) ||
            !bcd_is_valid(day) ||
            !bcd_is_valid(month) ||
            !bcd_is_valid(year)) {
            return -1;
        }

        second = bcd_to_binary(second);
        minute = bcd_to_binary(minute);
        day = bcd_to_binary(day);
        month = bcd_to_binary(month);
        year = bcd_to_binary(year);
        hour = (uint8_t)((hour & 0x80) |
                         bcd_to_binary(hour & 0x7F));

        if (bcd_is_valid(century)) {
            century = bcd_to_binary(century);
        } else {
            century = 0;
        }
    } else if (century > 99) {
        century = 0;
    }

    /* Handle both 12-hour and 24-hour RTC configurations. */
    if (!(status_b & CMOS_24_HOUR_MODE)) {
        uint8_t pm = hour & 0x80;
        hour &= 0x7F;

        if (hour < 1 || hour > 12) {
            return -1;
        }

        if (hour == 12) {
            hour = 0;
        }

        if (pm) {
            hour = (uint8_t)(hour + 12);
        }
    }

    time->second = second;
    time->minute = minute;
    time->hour = hour;
    time->day = day;
    time->month = month;
    time->year = rtc_expand_year(year, century);

    return rtc_date_is_valid(time) ? 0 : -1;
}

int rtc_get_time(struct rtc_time *time)
{
    struct rtc_time first;
    struct rtc_time second;
    unsigned int attempt;

    if (time == 0) {
        return -1;
    }

    /*
     * Read matching samples. Bounded retries ensure a broken or
     * uninitialized RTC cannot hang printk() indefinitely.
     */
    for (attempt = 0; attempt < RTC_READ_ATTEMPTS; attempt++) {
        if (rtc_read_raw(&first) != 0 ||
            rtc_read_raw(&second) != 0) {
            continue;
        }

        if (first.second != second.second ||
            first.minute != second.minute ||
            first.hour != second.hour ||
            first.day != second.day ||
            first.month != second.month ||
            first.year != second.year) {
            continue;
        }

        *time = second;
        return 0;
    }

    return -1;
}
