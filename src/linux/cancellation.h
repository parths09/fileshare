// cancellation.h
#pragma once
#include <atomic>

extern std::atomic<bool> cancel_requested; // Declaration
extern std::atomic<bool> connection_accepted; // Declaration

void signal_handler(int signum);           // Declaration
