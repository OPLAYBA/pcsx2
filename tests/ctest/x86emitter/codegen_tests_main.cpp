/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2020 PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "codegen_tests.h"
#include <gtest/gtest.h>
#include <common/emitter/x86emitter.h>
#include <cstdio>

using namespace x86Emitter;

TEST(CodegenTests, MOVTest)
{
	CODEGEN_TEST_BOTH(xMOV(rax, 0), "31 c0");
	CODEGEN_TEST_64(xMOV(rax, rcx), "48 89 c8");
	CODEGEN_TEST_BOTH(xMOV(eax, ecx), "89 c8");
	CODEGEN_TEST_64(xMOV(r8, 0), "45 31 c0");
	CODEGEN_TEST_64(xMOV(rax, r8), "4c 89 c0");
	CODEGEN_TEST_64(xMOV(r8, rax), "49 89 c0");
	CODEGEN_TEST_64(xMOV(r8, r9), "4d 89 c8");
	CODEGEN_TEST_64(xMOV(rax, ptrNative[rcx]), "48 8b 01");
	CODEGEN_TEST_BOTH(xMOV(eax, ptrNative[rcx]), "8b 01");
	CODEGEN_TEST_64(xMOV(ptrNative[rax], rcx), "48 89 08");
	CODEGEN_TEST_BOTH(xMOV(ptr32[rax], ecx), "89 08");
	CODEGEN_TEST_64(xMOV(rax, ptrNative[r8]), "49 8b 00");
	CODEGEN_TEST_64(xMOV(ptrNative[r8], rax), "49 89 00");
	CODEGEN_TEST_64(xMOV(r8, ptrNative[r9]), "4d 8b 01");
	CODEGEN_TEST_64(xMOV(ptrNative[r8], r9), "4d 89 08");
	CODEGEN_TEST_64(xMOV(rax, ptrNative[rbx*4+3+rcx]), "48 8b 44 99 03");
	CODEGEN_TEST_64(xMOV(ptrNative[rbx*4+3+rax], rcx), "48 89 4c 98 03");
	CODEGEN_TEST_BOTH(xMOV(eax, ptr32[rbx*4+3+rcx]), "8b 44 99 03");
	CODEGEN_TEST_BOTH(xMOV(ptr32[rbx*4+3+rax], ecx), "89 4c 98 03");
	CODEGEN_TEST_64(xMOV(r8, ptrNative[r10*4+3+r9]), "4f 8b 44 91 03");
	CODEGEN_TEST_64(xMOV(ptrNative[r9*4+3+r8], r10), "4f 89 54 88 03");
	CODEGEN_TEST_64(xMOV(ptrNative[r8], 0), "49 c7 00 00 00 00 00");
	CODEGEN_TEST_BOTH(xMOV(ptr32[rax], 0), "c7 00 00 00 00 00");
	CODEGEN_TEST_BOTH(xMOV(ptr32[rbx*4+3+rax], -1), "c7 44 98 03 ff ff ff ff");
	CODEGEN_TEST_64(xMOV(rax, 0xffffffff), "b8 ff ff ff ff");
	CODEGEN_TEST_64(xMOV(r8, -1), "49 c7 c0 ff ff ff ff");
	CODEGEN_TEST_64(xMOV64(rax, 0x1234567890), "48 b8 90 78 56 34 12 00 00 00");
	CODEGEN_TEST_64(xMOV64(r8, 0x1234567890), "49 b8 90 78 56 34 12 00 00 00");
	CODEGEN_TEST_64(xMOV(ptr32[base], 0x12), "c7 05 f6 ff ff ff 12 00 00 00");
	CODEGEN_TEST_BOTH(xMOVSX(eax, dx), "0f bf c2");
	CODEGEN_TEST_64(xMOVSX(rax, r8d), "49 63 c0");
	CODEGEN_TEST_64(xMOVSX(rax, ebx), "48 63 c3");
}

TEST(CodegenTests, LEATest)
{
	CODEGEN_TEST_64(xLEA(rax, ptr[rcx]), "48 89 c8"); // Converted to mov rax, rcx
	CODEGEN_TEST_BOTH(xLEA(eax, ptr[rcx]), "89 c8"); // Converted to mov eax, ecx
	CODEGEN_TEST_64(xLEA(rax, ptr[r8]), "4c 89 c0"); // Converted to mov rax, r8
	CODEGEN_TEST_64(xLEA(r8, ptr[r9]), "4d 89 c8"); // Converted to mov r8, r9
	CODEGEN_TEST_64(xLEA(rax, ptr[rbx*4+3+rcx]), "48 8d 44 99 03");
	CODEGEN_TEST_BOTH(xLEA(eax, ptr32[rbx*4+3+rcx]), "8d 44 99 03");
	CODEGEN_TEST_64(xLEA(r8, ptr[r10*4+3+r9]), "4f 8d 44 91 03");
	CODEGEN_TEST_64(xLEA(r8, ptr[base]), "4c 8d 05 f9 ff ff ff");
	CODEGEN_TEST_64(xLoadFarAddr(r8, base), "4c 8d 05 f9 ff ff ff");
	CODEGEN_TEST_64(xLoadFarAddr(r8, (void*)0x1234567890), "49 b8 90 78 56 34 12 00 00 00");
	CODEGEN_TEST_BOTH(xLEA(rax, ptr[(void*)0x1234]), "b8 34 12 00 00"); // Converted to mov rax, 0x1234
	CODEGEN_TEST_BOTH(xLoadFarAddr(rax, (void*)0x1234), "b8 34 12 00 00");
	CODEGEN_TEST(xLEA_Writeback(rbx), "bb cd cd cd cd", "48 8d 1d cd cd cd 0d");
}

TEST(CodegenTests, PUSHTest)
{
	CODEGEN_TEST_BOTH(xPUSH(rax), "50");
	CODEGEN_TEST_64(xPUSH(r8), "41 50");
	CODEGEN_TEST_BOTH(xPUSH(0x1234), "68 34 12 00 00");
	CODEGEN_TEST_BOTH(xPUSH(0x12), "6a 12");
	CODEGEN_TEST_BOTH(xPUSH(ptrNative[rax]), "ff 30");
	CODEGEN_TEST_64(xPUSH(ptrNative[r8]), "41 ff 30");
	CODEGEN_TEST_BOTH(xPUSH(ptrNative[rax*2+3+rbx]), "ff 74 43 03");
	CODEGEN_TEST_64(xPUSH(ptrNative[rax*2+3+r8]), "41 ff 74 40 03");
	CODEGEN_TEST_64(xPUSH(ptrNative[r9*4+3+r8]), "43 ff 74 88 03");
	CODEGEN_TEST_64(xPUSH(ptrNative[r8*4+3+rax]), "42 ff 74 80 03");
	CODEGEN_TEST_BOTH(xPUSH(ptrNative[rax*8+0x1234+rbx]), "ff b4 c3 34 12 00 00");
	CODEGEN_TEST_64(xPUSH(ptrNative[base]), "ff 35 fa ff ff ff");
	CODEGEN_TEST(xPUSH(ptrNative[(void*)0x1234]), "ff 35 34 12 00 00", "ff 34 25 34 12 00 00");
}

TEST(CodegenTests, POPTest)
{
	CODEGEN_TEST_BOTH(xPOP(rax), "58");
	CODEGEN_TEST_64(xPOP(r8), "41 58");
	CODEGEN_TEST_BOTH(xPOP(ptrNative[rax]), "8f 00");
	CODEGEN_TEST_64(xPOP(ptrNative[r8]), "41 8f 00");
	CODEGEN_TEST_BOTH(xPOP(ptrNative[rax*2+3+rbx]), "8f 44 43 03");
	CODEGEN_TEST_64(xPOP(ptrNative[rax*2+3+r8]), "41 8f 44 40 03");
	CODEGEN_TEST_64(xPOP(ptrNative[r9*4+3+r8]), "43 8f 44 88 03");
	CODEGEN_TEST_64(xPOP(ptrNative[r8*4+3+rax]), "42 8f 44 80 03");
	CODEGEN_TEST_BOTH(xPOP(ptrNative[rax*8+0x1234+rbx]), "8f 84 c3 34 12 00 00");
	CODEGEN_TEST_64(xPOP(ptrNative[base]), "8f 05 fa ff ff ff");
	CODEGEN_TEST(xPOP(ptrNative[(void*)0x1234]), "8f 05 34 12 00 00", "8f 04 25 34 12 00 00");
}

TEST(CodegenTests, MathTest)
{
	CODEGEN_TEST(xINC(eax), "40", "ff c0");
	CODEGEN_TEST(xDEC(rax), "48", "48 ff c8");
	CODEGEN_TEST_64(xINC(r8), "49 ff c0");
	CODEGEN_TEST_64(xADD(r8, r9), "4d 01 c8");
	CODEGEN_TEST_64(xADD(r8, 0x12), "49 83 c0 12");
	CODEGEN_TEST_64(xADD(rax, 0x1234), "48 05 34 12 00 00");
	CODEGEN_TEST_64(xADD(ptr8[base], 1), "80 05 f9 ff ff ff 01");
	CODEGEN_TEST_64(xADD(ptr32[base], -0x60), "83 05 f9 ff ff ff a0");
	CODEGEN_TEST_64(xADD(ptr32[base], 0x1234), "81 05 f6 ff ff ff 34 12 00 00");
	CODEGEN_TEST_BOTH(xADD(eax, ebx), "01 d8");
	CODEGEN_TEST_BOTH(xADD(eax, 0x1234), "05 34 12 00 00");
	CODEGEN_TEST_64(xADD(r8, ptrNative[r10*4+3+r9]), "4f 03 44 91 03");
	CODEGEN_TEST_64(xADD(ptrNative[r9*4+3+r8], r10), "4f 01 54 88 03");
	CODEGEN_TEST_BOTH(xADD(eax, ptr32[rbx*4+3+rcx]), "03 44 99 03");
	CODEGEN_TEST_BOTH(xADD(ptr32[rax*4+3+rbx], ecx), "01 4c 83 03");
	CODEGEN_TEST_64(xSUB(r8, 0x12), "49 83 e8 12");
	CODEGEN_TEST_64(xSUB(rax, 0x1234), "48 2d 34 12 00 00");
	CODEGEN_TEST_BOTH(xSUB(eax, ptr32[rcx*4+rax]), "2b 04 88");
	CODEGEN_TEST_64(xMUL(ptr32[base]), "f7 2d fa ff ff ff");
	CODEGEN_TEST(xMUL(ptr32[(void*)0x1234]), "f7 2d 34 12 00 00", "f7 2c 25 34 12 00 00");
	CODEGEN_TEST_BOTH(xDIV(ecx), "f7 f9");
}

TEST(CodegenTests, BitwiseTest)
{
	CODEGEN_TEST_64(xSHR(r8, cl), "49 d3 e8");
	CODEGEN_TEST_64(xSHR(rax, cl), "48 d3 e8");
	CODEGEN_TEST_BOTH(xSHR(ecx, cl), "d3 e9");
	CODEGEN_TEST_64(xSAR(r8, 1), "49 d1 f8");
	CODEGEN_TEST_64(xSAR(rax, 60), "48 c1 f8 3c");
	CODEGEN_TEST_BOTH(xSAR(eax, 30), "c1 f8 1e");
	CODEGEN_TEST_BOTH(xSHL(ebx, 30), "c1 e3 1e");
	CODEGEN_TEST_64(xSHL(ptr32[base], 4), "c1 25 f9 ff ff ff 04");
	CODEGEN_TEST_64(xAND(r8, r9), "4d 21 c8");
	CODEGEN_TEST_64(xXOR(rax, ptrNative[r10]), "49 33 02");
	CODEGEN_TEST_BOTH(xOR(esi, ptr32[rax+rbx]), "0b 34 18");
	CODEGEN_TEST_64(xNOT(r8), "49 f7 d0");
	CODEGEN_TEST_64(xNOT(ptrNative[rax]), "48 f7 10");
	CODEGEN_TEST_BOTH(xNOT(ptr32[rbx]), "f7 13");
}

TEST(CodegenTests, JmpTest)
{
	CODEGEN_TEST_64(xJMP(r8), "41 ff e0");
	CODEGEN_TEST_BOTH(xJMP(rdi), "ff e7");
	CODEGEN_TEST_BOTH(xJMP(ptrNative[rax]), "ff 20");
	CODEGEN_TEST_BOTH(xJA(base), "77 fe");
	CODEGEN_TEST_BOTH(xJB((char*)base - 0xFFFF), "0f 82 fb ff fe ff");
}

TEST(CodegenTests, SSETest)
{
	CODEGEN_TEST_BOTH(xMOVAPS(xmm0, xmm1), "0f 28 c1");
	CODEGEN_TEST_64(xMOVAPS(xmm8, xmm9), "45 0f 28 c1");
	CODEGEN_TEST_64(xMOVUPS(xmm8, ptr128[r8+r9]), "47 0f 10 04 08");
	CODEGEN_TEST_64(xMOVAPS(ptr128[rax+r9], xmm8), "46 0f 29 04 08");
	CODEGEN_TEST_BOTH(xBLEND.PS(xmm0, xmm1, 0x55), "66 0f 3a 0c c1 55");
	CODEGEN_TEST_64(xBLEND.PD(xmm8, xmm9, 0xaa), "66 45 0f 3a 0d c1 aa");
	CODEGEN_TEST_64(xEXTRACTPS(ptr32[base], xmm1, 2), "66 0f 3a 17 0d f6 ff ff ff 02");
	CODEGEN_TEST_64(xMOVD(eax, xmm1), "66 0f 7e c8");
	CODEGEN_TEST_64(xMOVD(eax, xmm10), "66 44 0f 7e d0");
	CODEGEN_TEST_64(xMOVD(rax, xmm1), "66 48 0f 7e c8");
	CODEGEN_TEST_64(xMOVD(r10, xmm1), "66 49 0f 7e ca");
	CODEGEN_TEST_64(xMOVD(rax, xmm10), "66 4c 0f 7e d0");
	CODEGEN_TEST_64(xMOVD(r10, xmm10), "66 4d 0f 7e d2");
}

TEST(CodegenTests, AVXTest)
{
	CODEGEN_TEST_64(xVMOVAPS(xmm0, xmm1), "c5 f8 28 c1");
	CODEGEN_TEST_64(xVMOVAPS(xmm0, ptr32[rdi]), "c5 f8 28 07");
	CODEGEN_TEST_64(xVMOVAPS(ptr32[rdi], xmm0), "c5 f8 29 07");
	CODEGEN_TEST_64(xVMOVUPS(xmm0, ptr32[rdi]), "c5 f8 10 07");
	CODEGEN_TEST_64(xVMOVUPS(ptr32[rdi], xmm0), "c5 f8 11 07");

	CODEGEN_TEST_64(xVADD.PS(xmm0, xmm1, xmm2), "c5 f0 58 c2");
	CODEGEN_TEST_64(xVADD.PD(xmm0, xmm1, xmm2), "c5 f1 58 c2");
	CODEGEN_TEST_64(xVADD.SS(xmm0, xmm1, xmm2), "c5 f2 58 c2");
	CODEGEN_TEST_64(xVADD.SD(xmm0, xmm1, xmm2), "c5 f3 58 c2");
	CODEGEN_TEST_64(xVSUB.PS(xmm0, xmm1, xmm2), "c5 f0 5c c2");
	CODEGEN_TEST_64(xVSUB.PD(xmm0, xmm1, xmm2), "c5 f1 5c c2");
	CODEGEN_TEST_64(xVSUB.SS(xmm0, xmm1, xmm2), "c5 f2 5c c2");
	CODEGEN_TEST_64(xVSUB.SD(xmm0, xmm1, xmm2), "c5 f3 5c c2");
	CODEGEN_TEST_64(xVMUL.PS(xmm0, xmm1, xmm2), "c5 f0 59 c2");
	CODEGEN_TEST_64(xVMUL.PD(xmm0, xmm1, xmm2), "c5 f1 59 c2");
	CODEGEN_TEST_64(xVMUL.SS(xmm0, xmm1, xmm2), "c5 f2 59 c2");
	CODEGEN_TEST_64(xVMUL.SD(xmm0, xmm1, xmm2), "c5 f3 59 c2");
	CODEGEN_TEST_64(xVDIV.PS(xmm0, xmm1, xmm2), "c5 f0 5e c2");
	CODEGEN_TEST_64(xVDIV.PD(xmm0, xmm1, xmm2), "c5 f1 5e c2");
	CODEGEN_TEST_64(xVDIV.SS(xmm0, xmm1, xmm2), "c5 f2 5e c2");
	CODEGEN_TEST_64(xVDIV.SD(xmm0, xmm1, xmm2), "c5 f3 5e c2");

	// Don't need to test all variants, since they just change the condition immediate.
	CODEGEN_TEST_64(xVCMP.EQ.PS(xmm0, xmm1, xmm2), "c5 f0 c2 c2 00");
	CODEGEN_TEST_64(xVCMP.EQ.PD(xmm0, xmm1, xmm2), "c5 f1 c2 c2 00");
	CODEGEN_TEST_64(xVCMP.EQ.SS(xmm0, xmm1, xmm2), "c5 f2 c2 c2 00");
	CODEGEN_TEST_64(xVCMP.EQ.SD(xmm0, xmm1, xmm2), "c5 f3 c2 c2 00");
	CODEGEN_TEST_64(xVCMP.LE.PS(xmm0, xmm1, xmm2), "c5 f0 c2 c2 02");
	CODEGEN_TEST_64(xVCMP.LE.PD(xmm0, xmm1, xmm2), "c5 f1 c2 c2 02");
	CODEGEN_TEST_64(xVCMP.LE.SS(xmm0, xmm1, xmm2), "c5 f2 c2 c2 02");
	CODEGEN_TEST_64(xVCMP.LE.SD(xmm0, xmm1, xmm2), "c5 f3 c2 c2 02");

	CODEGEN_TEST_64(xVPCMP.EQB(xmm0, xmm1, xmm2), "c5 f1 74 c2");
	CODEGEN_TEST_64(xVPCMP.EQW(xmm0, xmm1, xmm2), "c5 f1 75 c2");
	CODEGEN_TEST_64(xVPCMP.EQD(xmm0, xmm1, xmm2), "c5 f1 76 c2");
	CODEGEN_TEST_64(xVPCMP.GTB(xmm0, xmm1, xmm2), "c5 f1 64 c2");
	CODEGEN_TEST_64(xVPCMP.GTW(xmm0, xmm1, xmm2), "c5 f1 65 c2");
	CODEGEN_TEST_64(xVPCMP.GTD(xmm0, xmm1, xmm2), "c5 f1 66 c2");

	CODEGEN_TEST_64(xVPAND(xmm0, xmm1, xmm2), "c5 f1 db c2");
	CODEGEN_TEST_64(xVPANDN(xmm0, xmm1, xmm2), "c5 f1 df c2");
	CODEGEN_TEST_64(xVPOR(xmm0, xmm1, xmm2), "c5 f1 eb c2");
	CODEGEN_TEST_64(xVPXOR(xmm0, xmm1, xmm2), "c5 f1 ef c2");

	CODEGEN_TEST_64(xVMOVMSKPS(eax, xmm1), "c5 f8 50 c1");
	CODEGEN_TEST_64(xVMOVMSKPD(eax, xmm1), "c5 f9 50 c1");
}

TEST(CodegenTests, AVX256Test)
{
	CODEGEN_TEST_64(xVMOVAPS(ymm0, ymm1), "c5 fc 28 c1");
	CODEGEN_TEST_64(xVMOVAPS(ymm0, ptr32[rdi]), "c5 fc 28 07");
	CODEGEN_TEST_64(xVMOVAPS(ptr32[rdi], ymm0), "c5 fc 29 07");
	CODEGEN_TEST_64(xVMOVUPS(ymm0, ptr32[rdi]), "c5 fc 10 07");
	CODEGEN_TEST_64(xVMOVUPS(ptr32[rdi], ymm0), "c5 fc 11 07");

	CODEGEN_TEST_64(xVZEROUPPER(), "c5 f8 77");

	CODEGEN_TEST_64(xVADD.PS(ymm0, ymm1, ymm2), "c5 f4 58 c2");
	CODEGEN_TEST_64(xVADD.PD(ymm0, ymm1, ymm2), "c5 f5 58 c2");
	CODEGEN_TEST_64(xVSUB.PS(ymm0, ymm1, ymm2), "c5 f4 5c c2");
	CODEGEN_TEST_64(xVSUB.PD(ymm0, ymm1, ymm2), "c5 f5 5c c2");
	CODEGEN_TEST_64(xVMUL.PS(ymm0, ymm1, ymm2), "c5 f4 59 c2");
	CODEGEN_TEST_64(xVMUL.PD(ymm0, ymm1, ymm2), "c5 f5 59 c2");
	CODEGEN_TEST_64(xVDIV.PS(ymm0, ymm1, ymm2), "c5 f4 5e c2");
	CODEGEN_TEST_64(xVDIV.PD(ymm0, ymm1, ymm2), "c5 f5 5e c2");

	CODEGEN_TEST_64(xVCMP.EQ.PS(ymm0, ymm1, ymm2), "c5 f4 c2 c2 00");
	CODEGEN_TEST_64(xVCMP.EQ.PD(ymm0, ymm1, ymm2), "c5 f5 c2 c2 00");
	CODEGEN_TEST_64(xVCMP.LE.PS(ymm0, ymm1, ymm2), "c5 f4 c2 c2 02");
	CODEGEN_TEST_64(xVCMP.LE.PD(ymm0, ymm1, ymm2), "c5 f5 c2 c2 02");

	CODEGEN_TEST_64(xVPCMP.EQB(ymm0, ymm1, ymm2), "c5 f5 74 c2");
	CODEGEN_TEST_64(xVPCMP.EQW(ymm0, ymm1, ymm2), "c5 f5 75 c2");
	CODEGEN_TEST_64(xVPCMP.EQD(ymm0, ymm1, ymm2), "c5 f5 76 c2");
	CODEGEN_TEST_64(xVPCMP.GTB(ymm0, ymm1, ymm2), "c5 f5 64 c2");
	CODEGEN_TEST_64(xVPCMP.GTW(ymm0, ymm1, ymm2), "c5 f5 65 c2");
	CODEGEN_TEST_64(xVPCMP.GTD(ymm0, ymm1, ymm2), "c5 f5 66 c2");

	CODEGEN_TEST_64(xVPAND(ymm0, ymm1, ymm2), "c5 f5 db c2");
	CODEGEN_TEST_64(xVPANDN(ymm0, ymm1, ymm2), "c5 f5 df c2");
	CODEGEN_TEST_64(xVPOR(ymm0, ymm1, ymm2), "c5 f5 eb c2");
	CODEGEN_TEST_64(xVPXOR(ymm0, ymm1, ymm2), "c5 f5 ef c2");

	CODEGEN_TEST_64(xVMOVMSKPS(eax, ymm1), "c5 fc 50 c1");
	CODEGEN_TEST_64(xVMOVMSKPD(eax, ymm1), "c5 fd 50 c1");
}
