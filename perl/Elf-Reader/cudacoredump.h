/*
 * Copyright 2007-2017 NVIDIA Corporation.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __CUDACOREDUMP_H__
#define __CUDACOREDUMP_H__
#include "cuda_stdint.h"

typedef struct {
    uint64_t devName;           /* index into the string table */
    uint64_t devType;           /* index into the string table */
    uint64_t smType;            /* index into the string table */
    uint32_t devId;
    uint32_t pciBusId;
    uint32_t pciDevId;
    uint32_t numSMs;
    uint32_t numWarpsPerSM;
    uint32_t numLanesPerWarp;
    uint32_t numRegsPerLane;
    uint32_t numPredicatesPrLane;
    uint32_t smMajor;
    uint32_t smMinor;
    uint32_t instructionSize;   /* instruction size in bytes */
    uint32_t status;
    uint32_t numUniformRegsPrWarp;
    uint32_t numUniformPredicatesPrWarp;
} CudbgDeviceTableEntry;

// since r575
struct CudbgDeviceTableEntry_575: public CudbgDeviceTableEntry {
  /* Maximum number of convergence barriers per warp */
  uint32_t numConvergenceBarriersPrWarp;
  /* Padding, ignore */
  uint32_t padding0;
};

typedef struct {
    uint64_t gridId64;           /* 64-bit grid ID */
    uint64_t contextId;
    uint64_t function;
    uint64_t functionEntry;
    uint64_t moduleHandle;
    uint64_t parentGridId64;
    uint64_t paramsOffset;
    uint32_t kernelType;
    uint32_t origin;
    uint32_t gridStatus;
    uint32_t numRegs;
    uint32_t gridDimX;
    uint32_t gridDimY;
    uint32_t gridDimZ;
    uint32_t blockDimX;
    uint32_t blockDimY;
    uint32_t blockDimZ;
    uint32_t attrLaunchBlocking;
    uint32_t attrHostTid;
} CudbgGridTableEntry;

typedef struct {
    uint32_t smId;
    uint32_t pad;
} CudbgSmTableEntry;

typedef struct {
    uint64_t gridId64;         /* 64-bit grid ID */
    uint32_t blockIdxX;
    uint32_t blockIdxY;
    uint32_t blockIdxZ;
    uint32_t pad;
} CudbgCTATableEntry;

typedef struct {
    uint64_t errorPC;
    uint32_t warpId;
    uint32_t validLanesMask;
    uint32_t activeLanesMask;
    uint32_t isWarpBroken;     /* indicates if the warp is in
                                  brokenWarpsMask */
    uint32_t errorPCValid;
    uint32_t pad;
} CudbgWarpTableEntry;

typedef struct {
    uint64_t virtualPC;          /* virtualPC in the client's host VA */
    uint64_t physPC;             /* for gpudbgReadPC() */
    uint32_t ln;
    uint32_t threadIdxX;
    uint32_t threadIdxY;
    uint32_t threadIdxZ;
    uint32_t exception;          /* exception info for the lane */
    uint32_t callDepth;
    uint32_t syscallCallDepth;
    uint32_t ccRegister;
} CudbgThreadTableEntry;

typedef struct {
    uint64_t returnAddress;
    uint64_t virtualReturnAddress;
    uint32_t level;
    uint32_t pad;
} CudbgBacktraceTableEntry;

typedef struct {
    uint64_t moduleHandle;
} CudbgModuleTableEntry;

typedef struct {
    uint64_t contextId;
    uint64_t sharedWindowBase;
    uint64_t localWindowBase;
    uint64_t globalWindowBase;
    uint32_t deviceIdx;    /* index of an entry in the device table */
    uint32_t tid;          /* host thread id */
} CudbgContextTableEntry;

// since r565
typedef struct {
    /* Identifier for the generator of the coredump.
     * This field is an index into the string table.
     */
    uint64_t generatorName;
    /* The version of the GPU driver as reported by NVML API. Not set on Tegra. */
    uint32_t driverVersionMajor;
    uint32_t driverVersionMinor;
    /* The version of the CUDA driver as reported by the driver API (e.g. 12/7) */
    uint32_t cudaDriverVersionMajor;
    uint32_t cudaDriverVersionMinor;
    /* Flags used to generate the coredump (CUDBGCoredumpGenerationFlags) */
    uint32_t flags;
    /* Timestamp of this coredump, in seconds since the UNIX Epoch */
    uint32_t timestamp;
} CudbgMetaDataEntry;

// since r550
typedef struct {
    /* Global address of this constbank's start */
    uint64_t addr;
    /* Size of this constbank in bytes */
    uint32_t size;
    /* ID (number) of this constbank */
    uint32_t bankId;
} CudbgConstBankTableEntry;


#ifndef SHT_LOUSER
#define SHT_LOUSER    0x80000000
#endif

typedef enum {
    CUDBG_SHT_MANAGED_MEM = SHT_LOUSER + 1,
    CUDBG_SHT_GLOBAL_MEM  = SHT_LOUSER + 2,
    CUDBG_SHT_LOCAL_MEM   = SHT_LOUSER + 3,
    CUDBG_SHT_SHARED_MEM  = SHT_LOUSER + 4,
    CUDBG_SHT_DEV_REGS    = SHT_LOUSER + 5,
    CUDBG_SHT_ELF_IMG     = SHT_LOUSER + 6,
    CUDBG_SHT_RELF_IMG    = SHT_LOUSER + 7,
    CUDBG_SHT_BT          = SHT_LOUSER + 8,
    CUDBG_SHT_DEV_TABLE   = SHT_LOUSER + 9,
    CUDBG_SHT_CTX_TABLE   = SHT_LOUSER + 10,
    CUDBG_SHT_SM_TABLE    = SHT_LOUSER + 11,
    CUDBG_SHT_GRID_TABLE  = SHT_LOUSER + 12,
    CUDBG_SHT_CTA_TABLE   = SHT_LOUSER + 13,
    CUDBG_SHT_WP_TABLE    = SHT_LOUSER + 14,
    CUDBG_SHT_LN_TABLE    = SHT_LOUSER + 15,
    CUDBG_SHT_MOD_TABLE   = SHT_LOUSER + 16,
    CUDBG_SHT_DEV_PRED    = SHT_LOUSER + 17,
    CUDBG_SHT_PARAM_MEM   = SHT_LOUSER + 18,
    CUDBG_SHT_DEV_UREGS   = SHT_LOUSER + 19,
    CUDBG_SHT_DEV_UPRED   = SHT_LOUSER + 20,
/* Since CUDA Driver r550 */
    CUDBG_SHT_CB_TABLE    = SHT_LOUSER + 21,
    /* Since CUDA Driver r565 */
    CUDBG_SHT_META_DATA   = SHT_LOUSER + 22,
    /* Since CUDA Driver r575 */
    CUDBG_SHT_CBU_BAR     = SHT_LOUSER + 23,
} CudbgSectionHeaderTypes;

/* Section names */
#define CUDBG_SHNAME_GLOBAL     ".cudbg.global"
#define CUDBG_SHNAME_LOCAL      ".cudbg.local"
#define CUDBG_SHNAME_SHARED     ".cudbg.shared"
#define CUDBG_SHNAME_REGS       ".cudbg.regs"
#define CUDBG_SHNAME_PARAM      ".cudbg.param"
#define CUDBG_SHNAME_PRED       ".cudbg.pred"
#define CUDBG_SHNAME_DEVTABLE   ".cudbg.devtbl"
#define CUDBG_SHNAME_CTXTABLE   ".cudbg.ctxtbl"
#define CUDBG_SHNAME_SMTABLE    ".cudbg.smtbl"
#define CUDBG_SHNAME_GRIDTABLE  ".cudbg.gridtbl"
#define CUDBG_SHNAME_CTATABLE   ".cudbg.ctatbl"
#define CUDBG_SHNAME_WPTABLE    ".cudbg.wptbl"
#define CUDBG_SHNAME_LNTABLE    ".cudbg.lntbl"
#define CUDBG_SHNAME_BT         ".cudbg.bt"
#define CUDBG_SHNAME_MODTABLE   ".cudbg.modtbl"
#define CUDBG_SHNAME_ELFIMG     ".cudbg.elfimg"
#define CUDBG_SHNAME_RELFIMG    ".cudbg.relfimg"
#define CUDBG_SHNAME_UREGS      ".cudbg.uregs"
#define CUDBG_SHNAME_UPRED      ".cudbg.upred"
/* Since CUDA Driver r550 */
#define CUDBG_SHNAME_CBTABLE    ".cudbg.cbankstbl"
/* Since CUDA Driver r565 */
#define CUDBG_SHNAME_META_DATA  ".cudbg.meta"
/* Since CUDA Driver r575 */
#define CUDBG_SHNAME_CBU_BAR    ".cudbg.cbu_bar"

#endif
