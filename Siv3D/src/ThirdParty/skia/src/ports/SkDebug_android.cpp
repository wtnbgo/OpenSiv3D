/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Siv3D: Android 用の SkDebugf (上流の skia の src/ports/SkDebug_android.cpp と同じく logcat に出す)

#include "include/private/base/SkFeatures.h"
#include "include/private/base/SkLoadUserConfig.h"
#if defined(SK_BUILD_FOR_ANDROID)

#include <stdarg.h>
#include <android/log.h>

void SkDebugf(const char format[], ...) {
    va_list args;
    va_start(args, format);
    __android_log_vprint(ANDROID_LOG_DEBUG, "skia", format, args);
    va_end(args);
}

#endif // SK_BUILD_FOR_ANDROID
