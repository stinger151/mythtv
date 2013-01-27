#ifndef __UTIL_NVIDIA_H__
#define __UTIL_NVIDIA_H__

#include <map>
#include <vector>
#include <stdlib.h>
using namespace std;

typedef map<unsigned int, double> t_screenrate;

int GetNvidiaRates(t_screenrate& screenmap);
int CheckNVOpenGLSyncToVBlank(void);

#endif
