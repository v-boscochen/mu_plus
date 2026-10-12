/** @file
  Temporary direct-hardware Advanced Logger diagnostics.

  Copyright (C) Microsoft Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <Uefi/UefiBaseType.h>
#include <Library/AdvancedLoggerHdwPortLib.h>
#include <Library/DebugLib.h>

/**
  Write a diagnostic directly to hardware without invoking the RAM logger.

  @param[in] Message      Complete ASCII diagnostic.
  @param[in] MessageSize  Message length, excluding the terminator.
**/
STATIC
VOID
AdvancedLoggerTraceHardware (
  IN CONST CHAR8  *Message,
  IN UINTN        MessageSize
  )
{
  CHAR8  Failure[] = "[ALOG] TRACE_OUTPUT_INCOMPLETE\n";

  // DEBUG() here would recursively enter the logger being traced.
  if (AdvancedLoggerHdwPortWrite (DEBUG_ERROR, (UINT8 *)Message, MessageSize) != MessageSize) {
    AdvancedLoggerHdwPortWrite (DEBUG_ERROR, (UINT8 *)Failure, sizeof (Failure) - 1);
  }
}

#define ADVANCED_LOGGER_TRACE_HARDWARE(Message) \
  AdvancedLoggerTraceHardware (Message, sizeof (Message) - 1)
