#pragma once

#include "defines.h"

#ifndef SUBNET_CALC_APP
#define SUBNET_CALC_APP 0
#endif

#if SUBNET_CALC_APP
void initSubnetCalc();
void loopSubnetCalc();
void exitSubnetCalc();
#endif
