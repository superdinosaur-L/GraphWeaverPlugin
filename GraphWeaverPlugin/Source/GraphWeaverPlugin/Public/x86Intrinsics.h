// Copyright 2026 RainButterfly. All Rights Reserved.

#pragma once
//FPlatformMisc::HasAVX2InstructionSupport();
#include <intrin.h>

#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
	#define CPUID_X86_FAMILY 1
#else
	#define CPUID_X86_FAMILY 0
#endif

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
	#define CPUID_ARM_NEON 1
#else
	#define CPUID_ARM_NEON 0
#endif

inline bool HasSSE41()
{
#if CPUID_ARM_NEON
	return false;
#endif
	
	int cpuInfo[4];
#ifdef _MSC_VER
	__cpuid(cpuInfo, 1);
#else
	__cpuid_count(1, 0, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
#endif
	// ECX bit 19 = SSE4.1
	return (cpuInfo[2] & (1 << 19)) != 0;
}

inline bool HasAVX2()
{
#if CPUID_ARM_NEON
	return false;
#endif
	
	int cpuInfo[4];

	// CPU最高支持的CPUID Leaf
#ifdef _MSC_VER
	__cpuid(cpuInfo, 0);
#else
	__cpuid_count(0, 0, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
#endif
	if (cpuInfo[0] < 7)[[unlikely]]//Leaf == 7 -> AVX2,AVX512
		return false;

	// 检查AVX和OSXSAVE
	//返回EAX,EBX,ECX,EDX,  ECX: bit19->SSE4.1, bit20->SSE4.2, bit27->OSXSAVE, bit28->AVX
#ifdef _MSC_VER
	__cpuid(cpuInfo, 1);
#else
	__cpuid_count(1, 0, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
#endif
	const bool osUsesXSAVE = (cpuInfo[2] & (1 << 27)) != 0;
	const bool cpuSupportsAVX = (cpuInfo[2] & (1 << 28)) != 0;

	if (!osUsesXSAVE || !cpuSupportsAVX)
		return false;

	// Windows是否保存YMM寄存器
	//获取XCR0, bit0->基础浮点,必须为1. bit1->XMM0~XMM15,必须为1. bit2->AVX
#ifdef _MSC_VER
	const unsigned long long xcr0 = _xgetbv(0);
#else
	const unsigned long long xcr0 = __builtin_ia32_xgetbv(0);
#endif
	if ((xcr0 & 0x6) != 0x6)
		return false;

	//Leaf7 EBX 的subLeaf0中的bit5->AVX2
#ifdef _MSC_VER
	__cpuidex(cpuInfo, 7, 0);
#else
	__cpuid_count(7, 0, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
#endif
	return (cpuInfo[1] & (1 << 5)) != 0;
}

























