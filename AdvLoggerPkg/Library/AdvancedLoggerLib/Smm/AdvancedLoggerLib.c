/** @file
  SMM implementation of Advanced Logger Library.

  Copyright (c) Microsoft Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <AdvancedLoggerInternal.h>

#include <Protocol/AdvancedLogger.h>

#include <Library/DebugLib.h>
#include <Library/SmmServicesTableLib.h>

#include "../AdvancedLoggerTrace.h"

STATIC BOOLEAN                   mInitialized = FALSE;
STATIC ADVANCED_LOGGER_PROTOCOL  *mSmmLoggerProtocol;

/**
    Get the Logger Information Block published by SmmCore.
 **/
STATIC
VOID
SmmInitializeLoggerInfo (
  VOID
  )
{
  EFI_STATUS  Status;

  ADVANCED_LOGGER_TRACE_HARDWARE ("[SMM-ALOG][Smm] SmmInitializeLoggerInfo: ENTER\n");
  if (gSmst == NULL) {
    ADVANCED_LOGGER_TRACE_HARDWARE ("[SMM-ALOG][Smm] SmmInitializeLoggerInfo: NO_SMM_SERVICES\n");
    return;
  }

  if (!mInitialized) {
    //
    // Locate the Logger Information block.
    //

    mInitialized = TRUE;                // Only one attempt at getting the logger info block.

    ADVANCED_LOGGER_TRACE_HARDWARE ("[SMM-ALOG][Smm] SmmInitializeLoggerInfo: LOCATE_SMM_PROTOCOL\n");
    Status = gSmst->SmmLocateProtocol (
                      &gAdvancedLoggerProtocolGuid,
                      NULL,
                      (VOID **)&mSmmLoggerProtocol
                      );

    if (EFI_ERROR (Status)) {
      ADVANCED_LOGGER_TRACE_HARDWARE ("[SMM-ALOG][Smm] SmmInitializeLoggerInfo: PROTOCOL_LOOKUP_FAILED\n");
      mSmmLoggerProtocol = NULL;
    } else {
      ASSERT (mSmmLoggerProtocol->Signature == ADVANCED_LOGGER_PROTOCOL_SIGNATURE);
      ASSERT (mSmmLoggerProtocol->Version == ADVANCED_LOGGER_PROTOCOL_VERSION);
    }

    //
    // If mSmmLoggerProtocol is NULL at this point, there is no Advanced Logger for SMM modules.
    //

    DEBUG ((DEBUG_INFO, "%a: SmmLoggerProtocol=%p, code=%r\n", __func__, mSmmLoggerProtocol, Status));
  }
}

/**
  Advanced Logger Write

  @param  ErrorLevel      The error level of the debug message.
  @param  Buffer          The debug message to log.
  @param  NumberOfBytes   Number of bytes in the debug message.

**/
VOID
EFIAPI
AdvancedLoggerWrite (
  IN        UINTN  ErrorLevel,
  IN  CONST CHAR8  *Buffer,
  IN        UINTN  NumberOfBytes
  )
{
  ADVANCED_LOGGER_TRACE_HARDWARE ("[SMM-ALOG][Smm] AdvancedLoggerWrite: ENTER\n");
  SmmInitializeLoggerInfo ();

  if (mSmmLoggerProtocol != NULL) {
    ADVANCED_LOGGER_TRACE_HARDWARE ("[SMM-ALOG][Smm] AdvancedLoggerWrite: FORWARD_TO_SMM_CORE\n");
    mSmmLoggerProtocol->AdvancedLoggerWriteProtocol (mSmmLoggerProtocol, ErrorLevel, Buffer, NumberOfBytes);
  } else {
    ADVANCED_LOGGER_TRACE_HARDWARE ("[SMM-ALOG][Smm] AdvancedLoggerWrite: NO_SMM_PROTOCOL\n");
  }
}

/**
  The constructor function initializes Logger Information pointer to ensure that the
  pointer is initialized in DXE - either by the constructor, or the first DEBUG message.

  @param  ImageHandle   The firmware allocated handle for the EFI image.
  @param  SystemTable   A pointer to the EFI System Table.

  @retval EFI_SUCCESS   The constructor always returns EFI_SUCCESS.

**/
EFI_STATUS
EFIAPI
SmmAdvancedLoggerLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  ADVANCED_LOGGER_TRACE_HARDWARE ("[SMM-ALOG][Smm] SmmAdvancedLoggerLibConstructor: ENTER\n");
  SmmInitializeLoggerInfo ();

  return EFI_SUCCESS;
}
