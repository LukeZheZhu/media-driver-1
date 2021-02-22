/*===================== begin_copyright_notice ==================================

Copyright (c) 2017-2020, Intel Corporation

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

======================= end_copyright_notice ==================================*/
//!
//! \file     media_sysinfo_g12.cpp
//!

#include "igfxfmid.h"
#include "linux_system_info.h"
#include "skuwa_factory.h"
#include "linux_skuwa_debug.h"
#include "linux_media_skuwa.h"
#include "linux_shadow_skuwa.h"
#include "mos_utilities.h"

//extern template class DeviceInfoFactory<GfxDeviceInfo>;
typedef DeviceInfoFactory<GfxDeviceInfo> base_fact;

#define THREADS_NUMBER_PER_EU 7

static bool InitTglMediaSysInfo(struct GfxDeviceInfo *devInfo, MEDIA_GT_SYSTEM_INFO *sysInfo)
{
    if ((devInfo == nullptr) || (sysInfo == nullptr))
    {
        DEVINFO_ERROR("null ptr is passed\n");
        return false;
    }

    if (!sysInfo->SliceCount)
    {
        sysInfo->SliceCount    = devInfo->SliceCount;
    }

    if (!sysInfo->SubSliceCount)
    {
        sysInfo->SubSliceCount = devInfo->SubSliceCount;
    }

    if (!sysInfo->EUCount)
    {
        sysInfo->EUCount       = devInfo->EUCount;
    }

    sysInfo->L3BankCount                            = devInfo->L3BankCount;
    sysInfo->VEBoxInfo.Instances.Bits.VEBox0Enabled = 1;
    sysInfo->MaxEuPerSubSlice = devInfo->MaxEuPerSubSlice;
    sysInfo->MaxSlicesSupported = sysInfo->SliceCount;
    sysInfo->MaxSubSlicesSupported = sysInfo->SubSliceCount;

    sysInfo->VEBoxInfo.NumberOfVEBoxEnabled = 0; /*Query the VEBox engine info from KMD*/
    sysInfo->VDBoxInfo.NumberOfVDBoxEnabled = 0; /*Query the VDBox engine info from KMD*/

    sysInfo->ThreadCount = sysInfo->EUCount * THREADS_NUMBER_PER_EU;

    sysInfo->VEBoxInfo.IsValid = true;
    sysInfo->VDBoxInfo.IsValid = true;

    //Media driver does not set the other gtsysinfo fileds such as L3CacheSizeInKb, EdramSizeInKb and LLCCacheSizeInKb now.
    //If needed in the future, query them from KMD.

    return true;
}

static bool InitTglShadowSku(struct GfxDeviceInfo *devInfo,
                             SHADOW_MEDIA_FEATURE_TABLE *skuTable,
                             struct LinuxDriverInfo *drvInfo)
{
    if ((devInfo == nullptr) || (skuTable == nullptr) || (drvInfo == nullptr))
    {
        DEVINFO_ERROR("null ptr is passed\n");
        return false;
    }

    skuTable->FtrVERing = 0;
    if (drvInfo->hasVebox)
    {
       skuTable->FtrVERing = 1;
    }

    skuTable->FtrVcs2 = 0;

    skuTable->FtrULT = 0;

    skuTable->FtrPPGTT = 1;
    skuTable->FtrIA32eGfxPTEs = 1;

    skuTable->FtrDisplayYTiling = 1;
    skuTable->FtrEDram = devInfo->hasERAM;

    bool enableCodecMMC = false;
    bool enableVPMMC    = false;
    bool disableMMC     = false;
    skuTable->FtrE2ECompression = 1;
    // Disable MMC for all components if set reg key
    MOS_USER_FEATURE_VALUE_DATA userFeatureData;
    MOS_ZeroMemory(&userFeatureData, sizeof(userFeatureData));
    MOS_UserFeature_ReadValue_ID(
        nullptr,
        __MEDIA_USER_FEATURE_VALUE_DISABLE_MMC_ID,
        &userFeatureData,
        nullptr);
    if (userFeatureData.bData)
    {
        disableMMC = true;
    }

    if (disableMMC)
    {
        skuTable->FtrE2ECompression = 0;
    }

    skuTable->FtrLinearCCS = 1;
    skuTable->FtrTileY = 1;

    return true;
}

static bool InitTglShadowWa(struct GfxDeviceInfo *devInfo,
                             SHADOW_MEDIA_WA_TABLE *waTable,
                             struct LinuxDriverInfo *drvInfo)
{
    if ((devInfo == nullptr) || (waTable == nullptr) || (drvInfo == nullptr))
    {
        DEVINFO_ERROR("null ptr is passed\n");
        return false;
    }

    /* by default PPGTT is enabled */
    waTable->WaForceGlobalGTT = 0;
    if (drvInfo->hasPpgtt == 0)
    {
        waTable->WaForceGlobalGTT = 1;
    }

    waTable->WaDisregardPlatformChecks          = 1;
    waTable->Wa4kAlignUVOffsetNV12LinearSurface = 1;

    // Set it to 0 if need to support 256B compress mode
    waTable->WaLimit128BMediaCompr = 0;

    //source and recon surfaces need to be aligned to the LCU size
    waTable->WaAlignYUVResourceToLCU = 1;

    return true;
}

#ifdef IGFX_GEN12_DG1_SUPPORTED
static bool InitDG1ShadowSku(struct GfxDeviceInfo *devInfo,
                             SHADOW_MEDIA_FEATURE_TABLE *skuTable,
                             struct LinuxDriverInfo *drvInfo)
{
    if(!InitTglShadowSku(devInfo, skuTable, drvInfo))
    {
        return false;
    }
    skuTable->FtrLocalMemory = 1;

    return true;
}
#endif

static struct GfxDeviceInfo tgllpGt1Info = {
    .platformType  = PLATFORM_MOBILE,
    .productFamily = IGFX_TIGERLAKE_LP,
    .displayFamily = IGFX_GEN12_CORE,
    .renderFamily  = IGFX_GEN12_CORE,
    .eGTType       = GTTYPE_GT1,
    .L3CacheSizeInKb = 0,
    .L3BankCount   = 0,
    .EUCount       = 0,
    .SliceCount    = 0,
    .SubSliceCount = 0,
    .MaxEuPerSubSlice = 0,
    .isLCIA        = 0,
    .hasLLC        = 0,
    .hasERAM       = 0,
    .InitMediaSysInfo = InitTglMediaSysInfo,
    .InitShadowSku    = InitTglShadowSku,
    .InitShadowWa     = InitTglShadowWa,
};

static struct GfxDeviceInfo tgllpGt2Info = {
    .platformType  = PLATFORM_MOBILE,
    .productFamily = IGFX_TIGERLAKE_LP,
    .displayFamily = IGFX_GEN12_CORE,
    .renderFamily  = IGFX_GEN12_CORE,
    .eGTType       = GTTYPE_GT2,
    .L3CacheSizeInKb = 0,
    .L3BankCount   = 0,
    .EUCount       = 0,
    .SliceCount    = 0,
    .SubSliceCount = 0,
    .MaxEuPerSubSlice = 0,
    .isLCIA        = 0,
    .hasLLC        = 0,
    .hasERAM       = 0,
    .InitMediaSysInfo = InitTglMediaSysInfo,
    .InitShadowSku    = InitTglShadowSku,
    .InitShadowWa     = InitTglShadowWa,
};

#ifdef IGFX_GEN12_DG1_SUPPORTED
static struct GfxDeviceInfo dg1Gt2Info = {
    .platformType  = PLATFORM_MOBILE,
    .productFamily = IGFX_DG1,
    .displayFamily = IGFX_GEN12_CORE,
    .renderFamily  = IGFX_GEN12_CORE,
    .eGTType       = GTTYPE_GT2,
    .L3CacheSizeInKb = 0,
    .L3BankCount   = 0,
    .EUCount       = 0,
    .SliceCount    = 0,
    .SubSliceCount = 0,
    .MaxEuPerSubSlice = 0,
    .isLCIA        = 0,
    .hasLLC        = 0,
    .hasERAM       = 0,
    .InitMediaSysInfo = InitTglMediaSysInfo,
    .InitShadowSku    = InitDG1ShadowSku,
    .InitShadowWa     = InitTglShadowWa,
};
static bool dg1Gt2Device4905 = DeviceInfoFactory<GfxDeviceInfo>::
    RegisterDevice(0x4905, &dg1Gt2Info);

static bool dg1Gt2Device4906 = DeviceInfoFactory<GfxDeviceInfo>::
    RegisterDevice(0x4906, &dg1Gt2Info);


static bool dg1Gt2Device4907 = DeviceInfoFactory<GfxDeviceInfo>::
    RegisterDevice(0x4907, &dg1Gt2Info);

static bool dg1Gt2Device4908 = DeviceInfoFactory<GfxDeviceInfo>::
    RegisterDevice(0x4908, &dg1Gt2Info);
#endif

#ifdef IGFX_GEN12_RKL_SUPPORTED
static struct GfxDeviceInfo rklGt1Info = {
    .platformType     = PLATFORM_MOBILE,
    .productFamily    = IGFX_ROCKETLAKE,
    .displayFamily    = IGFX_GEN12_CORE,
    .renderFamily     = IGFX_GEN12_CORE,
    .eGTType          = GTTYPE_GT1,
    .L3CacheSizeInKb  = 0,
    .L3BankCount      = 0,
    .EUCount          = 0,
    .SliceCount       = 0,
    .SubSliceCount    = 0,
    .MaxEuPerSubSlice = 0,
    .isLCIA           = 0,
    .hasLLC           = 0,
    .hasERAM          = 0,
    .InitMediaSysInfo = InitTglMediaSysInfo,
    .InitShadowSku    = InitTglShadowSku,
    .InitShadowWa     = InitTglShadowWa,
};

static struct GfxDeviceInfo rklGt1fInfo = {
    .platformType     = PLATFORM_MOBILE,
    .productFamily    = IGFX_ROCKETLAKE,
    .displayFamily    = IGFX_GEN12_CORE,
    .renderFamily     = IGFX_GEN12_CORE,
    .eGTType          = GTTYPE_GT0_5,
    .L3CacheSizeInKb  = 0,
    .L3BankCount      = 0,
    .EUCount          = 0,
    .SliceCount       = 0,
    .SubSliceCount    = 0,
    .MaxEuPerSubSlice = 0,
    .isLCIA           = 0,
    .hasLLC           = 0,
    .hasERAM          = 0,
    .InitMediaSysInfo = InitTglMediaSysInfo,
    .InitShadowSku    = InitTglShadowSku,
    .InitShadowWa     = InitTglShadowWa,
};

static bool rklGt1Device4C80 = DeviceInfoFactory<GfxDeviceInfo>::
    RegisterDevice(0x4C80, &rklGt1Info);

static bool rklGt1Device4C8A = DeviceInfoFactory<GfxDeviceInfo>::
    RegisterDevice(0x4C8A, &rklGt1Info);

static bool rklGt1fDevice4C8B = DeviceInfoFactory<GfxDeviceInfo>::
    RegisterDevice(0x4C8B, &rklGt1Info);

static bool rklGt1fDevice4C8C = DeviceInfoFactory<GfxDeviceInfo>::
    RegisterDevice(0x4C8C, &rklGt1fInfo);

static bool rklGt1fDevice4C90 = DeviceInfoFactory<GfxDeviceInfo>::
    RegisterDevice(0x4C90, &rklGt1Info);

static bool rklGt1fDevice4C9A = DeviceInfoFactory<GfxDeviceInfo>::
    RegisterDevice(0x4C9A, &rklGt1Info);
#endif

#ifdef IGFX_GEN12_ADLS_SUPPORTED
static struct GfxDeviceInfo adlsGt1Info = {
    .platformType     = PLATFORM_DESKTOP,
    .productFamily    = IGFX_ALDERLAKE_S,
    .displayFamily    = IGFX_GEN12_CORE,
    .renderFamily     = IGFX_GEN12_CORE,
    .eGTType          = GTTYPE_GT1,
    .L3CacheSizeInKb  = 0,
    .L3BankCount      = 0,
    .EUCount          = 0,
    .SliceCount       = 0,
    .SubSliceCount    = 0,
    .MaxEuPerSubSlice = 0,
    .isLCIA           = 0,
    .hasLLC           = 0,
    .hasERAM          = 0,
    .InitMediaSysInfo = InitTglMediaSysInfo,
    .InitShadowSku    = InitTglShadowSku,
    .InitShadowWa     = InitTglShadowWa,
};


static struct GfxDeviceInfo adlsGt1fInfo = {
    .platformType     = PLATFORM_DESKTOP,
    .productFamily    = IGFX_ALDERLAKE_S,
    .displayFamily    = IGFX_GEN12_CORE,
    .renderFamily     = IGFX_GEN12_CORE,
    .eGTType          = GTTYPE_GT0_5,
    .L3CacheSizeInKb  = 0,
    .L3BankCount      = 0,
    .EUCount          = 0,
    .SliceCount       = 0,
    .SubSliceCount    = 0,
    .MaxEuPerSubSlice = 0,
    .isLCIA           = 0,
    .hasLLC           = 0,
    .hasERAM          = 0,
    .InitMediaSysInfo = InitTglMediaSysInfo,
    .InitShadowSku    = InitTglShadowSku,
    .InitShadowWa     = InitTglShadowWa,
};


static bool adlsGt1Device4680 = DeviceInfoFactory<GfxDeviceInfo>::
    RegisterDevice(0x4680, &adlsGt1Info);

static bool adlsGt1Device4681 = DeviceInfoFactory<GfxDeviceInfo>::
    RegisterDevice(0x4681, &adlsGt1Info);

static bool adlsGt1Device4682 = DeviceInfoFactory<GfxDeviceInfo>::
    RegisterDevice(0x4682, &adlsGt1Info);

static bool adlsGt1Device4683 = DeviceInfoFactory<GfxDeviceInfo>::
    RegisterDevice(0x4683, &adlsGt1fInfo);

static bool adlsGt1Device4690 = DeviceInfoFactory<GfxDeviceInfo>::
    RegisterDevice(0x4690, &adlsGt1Info);

static bool adlsGt1Device4691 = DeviceInfoFactory<GfxDeviceInfo>::
    RegisterDevice(0x4691, &adlsGt1Info);

static bool adlsGt1Device4692 = DeviceInfoFactory<GfxDeviceInfo>::
    RegisterDevice(0x4692, &adlsGt1Info);

static bool adlsGt1Device4693 = DeviceInfoFactory<GfxDeviceInfo>::
    RegisterDevice(0x4693, &adlsGt1Info);

#endif

static bool tgllpGt2Device9a40 = DeviceInfoFactory<GfxDeviceInfo>::
    RegisterDevice(0x9A40, &tgllpGt2Info);

static bool tgllpGt2Device9a49 = DeviceInfoFactory<GfxDeviceInfo>::
    RegisterDevice(0x9A49, &tgllpGt2Info);

static bool tgllpGt2Device9a59 = DeviceInfoFactory<GfxDeviceInfo>::
    RegisterDevice(0x9A59, &tgllpGt2Info);

static bool tgllpGt2Device9a60 = DeviceInfoFactory<GfxDeviceInfo>::
    RegisterDevice(0x9A60, &tgllpGt2Info);

static bool tgllpGt2Device9a68 = DeviceInfoFactory<GfxDeviceInfo>::
    RegisterDevice(0x9A68, &tgllpGt2Info);

static bool tgllpGt2Device9a70 = DeviceInfoFactory<GfxDeviceInfo>::
    RegisterDevice(0x9A70, &tgllpGt2Info);

static bool tgllpGt1Device9a78 = DeviceInfoFactory<GfxDeviceInfo>::
    RegisterDevice(0x9A78, &tgllpGt1Info);

static bool tgllpGt2Device9ac9 = DeviceInfoFactory<GfxDeviceInfo>::
    RegisterDevice(0x9AC9, &tgllpGt2Info);

static bool tgllpGt2Device9af8 = DeviceInfoFactory<GfxDeviceInfo>::
    RegisterDevice(0x9AF8, &tgllpGt2Info);

static bool tgllpGt2Device9ac0 = DeviceInfoFactory<GfxDeviceInfo>::
    RegisterDevice(0x9AC0, &tgllpGt2Info);

static bool tgllpGt2Device9ad9 = DeviceInfoFactory<GfxDeviceInfo>::
    RegisterDevice(0x9AD9, &tgllpGt2Info);

