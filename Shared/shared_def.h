/*******************************************************************************
 * file           : shared_def.h
 * brief          : Include file for shared code.
 ******************************************************************************/
#ifndef SHARED_DEF_H
#define SHARED_DEF_H

/**
 * Define SHARED_HEADER_INCLUDE in files.make. For example:
 * add_compile_definitions("SHARED_HEADER_INCLUDE=\"../Blink_LED/Blink_LED_cmake/shared.h\"")
 *
 * OR
 *
 * Define SHARED_HEADER_INCLUDE in flags.make. For example:
 * -D 'SHARED_HEADER_INCLUDE=\"../Blink_LED/Blink_LED_cmake/shared.h\"'
 */
#include SHARED_HEADER_INCLUDE

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SHARED_DEF_H */

