/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/*
 * @file       taf_pa_common_defines.hpp
 * @brief      The PA data macros and common defines across subsystems.
 */

#ifndef __TAF_PA_COMMON_DEFINES_HPP__
#define __TAF_PA_COMMON_DEFINES_HPP__

#include <sstream>   // For std::ostringstream
#include <string>    // For std::string
#include <thread>    // For std::this_thread::get_id()
#include <pthread.h> // For pthread_setname_np, pthread_self

//--------------------------------------------------------------------------------------------------
/**
 * Convert a ENUM to an integer primarily for printing with LE log APIs.
 *
 * Consider using the to_int template for more complex use cases.
 */
//--------------------------------------------------------------------------------------------------
#define TO_INT(value) static_cast<int>(value)

//--------------------------------------------------------------------------------------------------
/**
 * Template to check if a key exists in a std::map.
 *
 */
//--------------------------------------------------------------------------------------------------
template <typename MapType, typename KeyType>
bool IS_KEY_IN_MAP(const MapType &map, const KeyType &key)
{
    return map.find(key) != map.end();
}

//--------------------------------------------------------------------------------------------------
/**
 * @brief Return true if the given "value" is within the specified range
 */
//--------------------------------------------------------------------------------------------------
template <typename T>
bool IS_VALUE_IN_RANGE(T value, T lowerBound, T upperBound)
{
    return value >= lowerBound && value <= upperBound;
}

//--------------------------------------------------------------------------------------------------
/**
 * @brief Set the SDK thread name for easier debugging.
 */
//--------------------------------------------------------------------------------------------------
#define SET_SDK_THREAD_NAME()                                                                \
    do                                                                                       \
    {                                                                                        \
        std::ostringstream threadName;                                                       \
        threadName << "T" << std::this_thread::get_id();                                     \
        std::string fullName = threadName.str();                                             \
        /* Limit to 15 characters (including null terminator) */                             \
        std::string name = fullName.substr(0, 15);                                           \
        int result = pthread_setname_np(pthread_self(), name.c_str());                       \
        if ( 0 != result)                                                                    \
        {                                                                                    \
          PA_WARN("pthread_setname_np(%s) failed with error code %d", name.c_str(), result); \
        }                                                                                    \
} while (0);

//--------------------------------------------------------------------------------------------------
/**
 * @brief Use a try-catch when using future.get() and return the result.
 *
 * Use a future wait() or wait_for() before calling get() to handle long running tasks.
 *
 */
//--------------------------------------------------------------------------------------------------
#define FUTURE_GET_RET_VAL(future, futureResult, result) \
    try                                                         \
    {                                                           \
        futureResult = future.get();                            \
    }                                                           \
    catch (const std::exception &e)                             \
    {                                                           \
        PA_WARN("Exception caught: %s", e.what());              \
        return result;                                          \
    }                                                           \
    catch (...)                                                 \
    {                                                           \
        PA_WARN("Unknown exception caught");                    \
        return result;                                          \
    }

//--------------------------------------------------------------------------------------------------
/**
 * @brief Use a try-catch when using future.get() and simply return.
 *
 * Use a future wait() or wait_for() before calling get() to handle long running tasks.
 */
//--------------------------------------------------------------------------------------------------
#define FUTURE_GET_RET_NIL(future, futureResult) \
    try                                                 \
    {                                                   \
        futureResult = future.get();                    \
    }                                                   \
    catch (const std::exception &e)                     \
    {                                                   \
        PA_WARN("Exception caught: %s", e.what());      \
        return;                                         \
    }                                                   \
    catch (...)                                         \
    {                                                   \
        PA_WARN("Unknown exception caught");            \
        return;                                         \
    }

namespace taf
{
namespace pa
{

//--------------------------------------------------------------------------------------------------
/**
 * The timeout for subsystem initialization.
 */
//--------------------------------------------------------------------------------------------------
static constexpr int SUBSYSTEM_INIT_TIMEOUT = 30;

//--------------------------------------------------------------------------------------------------
/**
 * The timeout for network related commands (e.g start/stop data).
 */
//--------------------------------------------------------------------------------------------------
static constexpr int NETWORK_COMMAND_TIMEOUT = 60;

//--------------------------------------------------------------------------------------------------
/**
 * The timeout for non network related commands.
 */
//--------------------------------------------------------------------------------------------------
static constexpr int NON_NETWORK_COMMAND_TIMEOUT = 5;

} // pa
} // taf


#endif //__TAF_PA_COMMON_DEFINES_HPP__