/** @file
  SMM_CORE implementation of Advanced Logger Library.

  Copyright (c) Microsoft Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <AdvancedLoggerInternal.h>

#include <Protocol/AdvancedLogger.h>
#include <AdvancedLoggerInternalProtocol.h>

#include <Pi/PiSmmCis.h>

#include <Library/AdvancedLoggerLib.h>
#include <Library/DebugLib.h>
#include <Library/SynchronizationLib.h>
#include <Library/SmmServicesTableLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include "../AdvancedLoggerCommon.h"
#include "../AdvancedLoggerTrace.h"

STATIC ADVANCED_LOGGER_INFO  *mLoggerInfo;
STATIC UINT32                mBufferSize  = 0;
STATIC EFI_PHYSICAL_ADDRESS  mMaxAddress  = 0;
STATIC BOOLEAN               mInitialized = FALSE;

VOID
EFIAPI
AdvancedLoggerWriteProtocol (
  IN        ADVANCED_LOGGER_PROTOCOL  *This,
  IN        UINTN                     ErrorLevel,
  IN  CONST CHAR8                     *Buffer,
  IN        UINTN                     NumberOfBytes
  );

STATIC ADVANCED_LOGGER_PROTOCOL_CONTAINER  mAdvLoggerProtocol = {
  .AdvLoggerProtocol             = {
    .Signature                   = ADVANCED_LOGGER_PROTOCOL_SIGNATURE,
    .Version                     = ADVANCED_LOGGER_PROTOCOL_VERSION,
    .AdvancedLoggerWriteProtocol = AdvancedLoggerWriteProtocol
  },
  .LoggerInfo                    = NULL
};

/**
  AdvancedLoggerWriteProtocol

  @param  This            Pointer to Advanced Logger Protocol,
  @param  ErrorLevel      The error level of the debug message.
  @param  Buffer          The debug message to log.
  @param  NumberOfBytes   Number of bytes in the debug message.

**/
VOID
EFIAPI
AdvancedLoggerWriteProtocol (
  IN        ADVANCED_LOGGER_PROTOCOL  *This,
  IN        UINTN                     ErrorLevel,
  IN  CONST CHAR8                     *Buffer,
  IN        UINTN                     NumberOfBytes
  )
{
  ADVANCED_LOGGER_TRACE_HARDWARE ("[SMM-ALOG][SmmCore] AdvancedLoggerWriteProtocol: ENTER\n");
  AdvancedLoggerWrite (ErrorLevel, Buffer, NumberOfBytes);
}

/**
    CheckAddress

    The address of the ADVANCE_LOGGER_INFO block pointer is captured before END_OF_DXE.
    LogBuffer, LogCurrentOffset, and LogBufferSize could be written to by un trusted code.  Here, we check that
    the offsets are within the allocated mLoggerInfo space, and that LogBufferSize, which is used in multiple places
    to see if a new message will fit into the log buffer, is valid.

    @param          NONE

    @return         BOOLEAN     TRUE - mInforBlock passes security checks
    @return         BOOLEAN     FALSE- mInforBlock failed security checks

**/
STATIC
BOOLEAN
ValidateInfoBlock (
  VOID
  )
{
  ADVANCED_LOGGER_TRACE_HARDWARE ("[SMM-ALOG][SmmCore] ValidateInfoBlock: ENTER\n");
  if (mLoggerInfo == NULL) {
    ADVANCED_LOGGER_TRACE_HARDWARE ("[SMM-ALOG][SmmCore] ValidateInfoBlock: REJECT_NO_LOGGER\n");
    return FALSE;
  }

  if (mLoggerInfo->Signature != ADVANCED_LOGGER_SIGNATURE) {
    ADVANCED_LOGGER_TRACE_HARDWARE ("[SMM-ALOG][SmmCore] ValidateInfoBlock: REJECT_SIGNATURE\n");
    return FALSE;
  }

  if (mLoggerInfo->LogBufferOffset != EXPECTED_LOG_BUFFER_OFFSET (mLoggerInfo)) {
    ADVANCED_LOGGER_TRACE_HARDWARE ("[SMM-ALOG][SmmCore] ValidateInfoBlock: REJECT_BUFFER_OFFSET\n");
    return FALSE;
  }

  if ((PA_FROM_PTR (LOG_CURRENT_FROM_ALI (mLoggerInfo)) > mMaxAddress) ||
      (mLoggerInfo->LogCurrentOffset < mLoggerInfo->LogBufferOffset))
  {
    ADVANCED_LOGGER_TRACE_HARDWARE ("[SMM-ALOG][SmmCore] ValidateInfoBlock: REJECT_CURSOR_BOUNDS\n");
    return FALSE;
  }

  if (mBufferSize == 0) {
    ADVANCED_LOGGER_TRACE_HARDWARE ("[SMM-ALOG][SmmCore] ValidateInfoBlock: CACHE_BUFFER_SIZE\n");
    mBufferSize = mLoggerInfo->LogBufferSize;
  } else {
    if (mLoggerInfo->LogBufferSize != mBufferSize) {
      ADVANCED_LOGGER_TRACE_HARDWARE ("[SMM-ALOG][SmmCore] ValidateInfoBlock: REJECT_BUFFER_SIZE\n");
      return FALSE;
    }
  }

  ADVANCED_LOGGER_TRACE_HARDWARE ("[SMM-ALOG][SmmCore] ValidateInfoBlock: VALID\n");
  return TRUE;
}

/**
    Get the Logger Information Block published by DxeCore.
 **/
STATIC
VOID
SmmInitializeLoggerInfo (
  VOID
  )
{
  ADVANCED_LOGGER_PROTOCOL  *LoggerProtocol;
  EFI_STATUS                Status;

  ADVANCED_LOGGER_TRACE_HARDWARE ("[SMM-ALOG][SmmCore] SmmInitializeLoggerInfo: ENTER\n");
  if (!mInitialized) {
    //
    // Locate the Logger Information block.
    //
    if (gBS == NULL) {
      ADVANCED_LOGGER_TRACE_HARDWARE ("[SMM-ALOG][SmmCore] SmmInitializeLoggerInfo: NO_BOOT_SERVICES\n");
      return;
    }

    mInitialized = TRUE;                // Only one attempt at getting the logger info block.

    ADVANCED_LOGGER_TRACE_HARDWARE ("[SMM-ALOG][SmmCore] SmmInitializeLoggerInfo: LOCATE_DXE_PROTOCOL\n");
    Status = gBS->LocateProtocol (
                    &gAdvancedLoggerProtocolGuid,
                    NULL,
                    (VOID **)&LoggerProtocol
                    );
    ASSERT_EFI_ERROR (Status);
    if (!EFI_ERROR (Status)) {
      mLoggerInfo = LOGGER_INFO_FROM_PROTOCOL (LoggerProtocol);
      ASSERT (mLoggerInfo != NULL);
      if (mLoggerInfo != NULL) {
        mMaxAddress = LOG_MAX_ADDRESS (mLoggerInfo);
      }
    }

    //
    // If mLoggerInfo is NULL at this point, there is no Advanced Logger.
    //

    DEBUG ((DEBUG_INFO, "%a: LoggerInfo=%p, Code=%r\n", __func__, mLoggerInfo, Status));
  }

  if (!ValidateInfoBlock ()) {
    ADVANCED_LOGGER_TRACE_HARDWARE ("[SMM-ALOG][SmmCore] SmmInitializeLoggerInfo: INVALIDATE_LOGGER\n");
    mLoggerInfo = NULL;
  }
}

/**
  Return the Logger Information block

**/
ADVANCED_LOGGER_INFO *
EFIAPI
AdvancedLoggerGetLoggerInfo (
  VOID
  )
{
  ADVANCED_LOGGER_TRACE_HARDWARE ("[SMM-ALOG][SmmCore] AdvancedLoggerGetLoggerInfo: ENTER\n");
  SmmInitializeLoggerInfo ();

  if (mLoggerInfo == NULL) {
    ADVANCED_LOGGER_TRACE_HARDWARE ("[SMM-ALOG][SmmCore] AdvancedLoggerGetLoggerInfo: RETURN_NULL\n");
  } else {
    ADVANCED_LOGGER_TRACE_HARDWARE ("[SMM-ALOG][SmmCore] AdvancedLoggerGetLoggerInfo: RETURN_VALID\n");
  }

  return mLoggerInfo;
}

/**
  Helper function to return the log phase for each message.

  This function is intended to be used to distinguish between
  various types of modules.

  @return       Phase of current advanced logger instance.
**/
UINT16
EFIAPI
AdvancedLoggerGetPhase (
  VOID
  )
{
  return ADVANCED_LOGGER_PHASE_SMM_CORE;
}

/**
  The constructor function initializes Logger Information pointer to ensure that the
  pointer is initialized in DXE - either by the constructor, or the first DEBUG message.
  It is also responsible for publishing the Smm Advanced Logger protocol for SMM drivers.

  @param  ImageHandle   The firmware allocated handle for the EFI image.
  @param  SystemTable   A pointer to the EFI System Table.

  @retval EFI_SUCCESS   The constructor always returns EFI_SUCCESS.

**/
EFI_STATUS
EFIAPI
SmmCoreAdvancedLoggerLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_HANDLE  Handle;
  EFI_STATUS  Status;

  ADVANCED_LOGGER_TRACE_HARDWARE ("[SMM-ALOG][SmmCore] SmmCoreAdvancedLoggerLibConstructor: ENTER\n");
  ASSERT ((gBS != NULL) && (gSmst != NULL));

  SmmInitializeLoggerInfo ();

  //
  // SMM_CORE does not allow SmmInstallProtocolInterface until after the SmmInitializeMemoryServices
  // call from MemoryAllocationLib.  Leave MemoryAllocationLib in the INF to ensure that
  // MemoryAllocationLib constructor runs before Advanced Logger.
  //

  Handle = NULL;
  Status = gSmst->SmmInstallProtocolInterface (
                    &Handle,
                    &gAdvancedLoggerProtocolGuid,
                    EFI_NATIVE_INTERFACE,
                    &mAdvLoggerProtocol.AdvLoggerProtocol
                    );

  DEBUG ((DEBUG_INFO, "%a: LoggerInfo=%p, Code=%r\n", __func__, mLoggerInfo, Status));
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}
