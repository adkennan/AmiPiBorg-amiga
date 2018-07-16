
#include "protocol.h"

UWORD APB_CalculateChecksum(
    UWORD * data,
    UWORD length)
{
    ULONG     sum = 0;

    sum = APB_AddToChecksum(sum, data, length);

    return APB_CompleteChecksum(sum);
}

ULONG APB_AddToChecksum(
    ULONG sum,
    UWORD * data,
    UWORD length)
{

    while(length > 0) {
        sum += *data;
        data++;
        length -= 2;
    }

    return sum;
}

UWORD APB_CompleteChecksum(
    ULONG sum)
{
    while(sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }

    sum = ~sum;

    return (UWORD) (sum + 1);
}
