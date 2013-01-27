#ifndef __CHANNEL_SCAN_TYPES_H_
#define __CHANNEL_SCAN_TYPES_H_

typedef enum ServiceRequirements
{
    kRequireNothing = 0x0,
    kRequireVideo   = 0x1,
    kRequireAudio   = 0x2,
    kRequireAV      = 0x3,
    kRequireData    = 0x4,
} ServiceRequirements;

#endif // __CHANNEL_SCAN_TYPES_H_
