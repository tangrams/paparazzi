#pragma once

#include "platform.h"

void processNetworkQueue();
void finishUrlRequests();

void resetTimer(std::string _msg = std::string(""));
