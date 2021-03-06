// AddCodePEFile.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "stdio.h"
#include "Windows.h"

HANDLE fi, mapOb;
LPVOID point;

PIMAGE_DOS_HEADER dosHeader;
PIMAGE_NT_HEADERS peHeader;
PIMAGE_FILE_HEADER fileHeader;
PIMAGE_OPTIONAL_HEADER32 opHeader;
PIMAGE_SECTION_HEADER secHeader;
PIMAGE_DATA_DIRECTORY dataDic;
PIMAGE_EXPORT_DIRECTORY exp;
PIMAGE_IMPORT_DESCRIPTOR imp;
PIMAGE_IMPORT_BY_NAME iibn;

char text[] = "Hi, I'm here!";
char tMSB[] = "MessageBoxA";
char tDLL[] = "user32.dll";
BYTE codeASM[] = { 0x6A, 0x00, 0x6A, 0x00, 0x68, 0x6A, 0x00, 0xFF, 0x15, 0xB8, 0xFF, 0xE0 };
PBYTE oldDic = NULL, wb, newDic = NULL;
PDWORD eat, ent, itd, dww;
PWORD eot;
DWORD rawOffset, *currThunk, RVAfunction = 0x0, sizeIIDs = 0x0, imgTrnkData;
char *inDLL;

DWORD getNullSize(PIMAGE_SECTION_HEADER sec) {
	DWORD count = 0;
	PBYTE bc = (PBYTE)((DWORD)point + (DWORD)sec->PointerToRawData);
	for (int i = 0; i < (DWORD)(sec->SizeOfRawData); i++) {
		if (*bc == 0x0)
			count++;
		else
			count = 0;

		bc++;
	}

	if (count < 20)
		return 0;
	else
		return count - 20;
}

int main(int argc, char *argv[]) {

	if (argc < 2) {
		printf("Usage: ./AddCodePEFile.exe target.exe");
		ExitProcess(0);
	}

	// Load file
	fi = CreateFile(argv[1], GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (fi == INVALID_HANDLE_VALUE) {
		printf("==> Error: Can't open file!");
		ExitProcess(0);
	}

	// Mapping file to memory
	mapOb = CreateFileMapping(fi, NULL, PAGE_READWRITE, 0, 0, NULL);
	point = MapViewOfFile(mapOb, FILE_MAP_WRITE | FILE_MAP_READ, 0, 0, 0);

	// DOS header
	dosHeader = (PIMAGE_DOS_HEADER)point;

	// PE header
	peHeader = (PIMAGE_NT_HEADERS)((PBYTE)dosHeader + dosHeader->e_lfanew);

	// File Header
	fileHeader = (PIMAGE_FILE_HEADER)&peHeader->FileHeader;

	// check 'MZ', 'PE'
	if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
		printf("==> Error: DOS header failse!\n");
	else if (peHeader->Signature != IMAGE_NT_SIGNATURE)
		printf("==> Error: PE Header false!\n");
	else {

		// Optional Header
		opHeader = (PIMAGE_OPTIONAL_HEADER)&peHeader->OptionalHeader;
		if (opHeader->Magic == 0x020B) {
			printf("Error: File 64bit!");
			ExitProcess(0);
		}

		// Data Directory
		dataDic = opHeader->DataDirectory;

		//*********** Export Section *************
		/*if (dataDic[0].Size > 0) {
			secHeader = IMAGE_FIRST_SECTION(peHeader);

			for (int i = 0; i < fileHeader->NumberOfSections; i++) {
				if ((secHeader->VirtualAddress <= dataDic[0].VirtualAddress) && (secHeader->VirtualAddress + secHeader->Misc.VirtualSize > dataDic[0].VirtualAddress)) {
					break;
				}

				secHeader++;
			}

			rawOffset = (DWORD)point + secHeader->PointerToRawData;

			printf("[*] Export Data Section:\n");

			exp = (PIMAGE_EXPORT_DIRECTORY)(rawOffset + dataDic[0].VirtualAddress - secHeader->VirtualAddress);

			printf("- DLL name: %s\n", (char*)(rawOffset + exp->Name - secHeader->VirtualAddress));
			
			eat = (PDWORD)(rawOffset + exp->AddressOfFunctions - secHeader->VirtualAddress);
			eot = (PWORD)(rawOffset + exp->AddressOfNameOrdinals - secHeader->VirtualAddress);
			ent = (PDWORD)(rawOffset + exp->AddressOfNames - secHeader->VirtualAddress);

			for (DWORD i = 0; i < (DWORD)exp->NumberOfFunctions; i++) {
				printf("\tFuntion RVAs: %#x", eat[i]);

				for(DWORD j = 0; j < exp->NumberOfNames; j++)
					if (i == eot[j]) {
						printf("\tName: %s", (char*)(rawOffset + ent[j] - secHeader->VirtualAddress));
						break;
					}
				printf("\n");
			}
		}
		*/
		//*********** Import Sections ****************

		if (dataDic[1].Size > 0) {
			secHeader = IMAGE_FIRST_SECTION(peHeader);

			for (int i = 0; i < fileHeader->NumberOfSections; i++) {
				if ((secHeader->VirtualAddress <= dataDic[1].VirtualAddress) && (secHeader->VirtualAddress + secHeader->Misc.VirtualSize > dataDic[1].VirtualAddress)) {
					break;
				}

				secHeader++;
			}

			rawOffset = (DWORD)point + secHeader->PointerToRawData;

			printf("------------------------------------------------------\n");
			printf("[*] Import Data Section: \n");

			imp = (PIMAGE_IMPORT_DESCRIPTOR)(rawOffset + dataDic[1].VirtualAddress - secHeader->VirtualAddress);

			oldDic = (PBYTE)imp;

			while (imp->Characteristics != 0 || imp->FirstThunk != 0 || imp->ForwarderChain != 0 || imp->Name != 0 || imp->OriginalFirstThunk != 0 || imp->TimeDateStamp != 0) {

				printf("- DLL name: %s\n", (char*)(rawOffset + imp->Name - secHeader->VirtualAddress));
				sizeIIDs += 20;

				// check OFT
				if (imp->OriginalFirstThunk != 0)
					currThunk = (PDWORD)(rawOffset + imp->OriginalFirstThunk - secHeader->VirtualAddress);
				else
					currThunk = (PDWORD)(rawOffset + imp->FirstThunk - secHeader->VirtualAddress);

				while (*currThunk != 0) {

					imgTrnkData = *currThunk;

					// check ordinal
					if (imgTrnkData & IMAGE_ORDINAL_FLAG32)
						printf("\tOrdinal: %#x\n", imgTrnkData^IMAGE_ORDINAL_FLAG32);
					else {
						iibn = (PIMAGE_IMPORT_BY_NAME)(rawOffset + imgTrnkData - secHeader->VirtualAddress);
						printf("\tFunction: %s\n", iibn->Name);

						if ((imp->OriginalFirstThunk == 0) && (strcmp((char*)iibn->Name, tMSB) == 0)) {

							RVAfunction = (DWORD)currThunk - (DWORD)point - secHeader->PointerToRawData + secHeader->VirtualAddress;

						}
					}

					currThunk++;
				}
				
				imp++;
			}
		}

		printf("----------------------------------------------------------\n");

		/*secHeader = IMAGE_FIRST_SECTION(peHeader);
		for (int i = 0; i < fileHeader->NumberOfSections; i++) {
			printf("%#x\n", getNullSize(secHeader));
			printf("%x\n", secHeader->PointerToRawData);

			secHeader++;
		}*/

		// ****************** import MessageBoxA() function ***********
		if (RVAfunction == 0) {
			secHeader = IMAGE_FIRST_SECTION(peHeader);
			sizeIIDs += 20;
			
			for (int i = 0; i < fileHeader->NumberOfSections; i++) {

				//printf("%d", getNullSize(secHeader));

				if (((secHeader->Characteristics & 0x40) == 0x40) && (getNullSize(secHeader) > (sizeIIDs + 20 + 12 + 8 + 14))) {

					DWORD sizeNull = getNullSize(secHeader);

					wb = (PBYTE)((DWORD)point + secHeader->SizeOfRawData + secHeader->PointerToRawData - sizeNull);

					// add string "user32.dll\0\0"
					inDLL = (char*)wb;
					strcpy_s(inDLL,20, tDLL);
					wb += 11;
					*wb = 0x0;
					wb++;

					// create IMAGE_IMPORT_BY_NAME
					iibn = (PIMAGE_IMPORT_BY_NAME)wb;
					*((PWORD)wb) = 0;
					wb += sizeof(WORD);
					strcpy_s((char*)wb, 20, tMSB);
					wb += 12;

					// create IMAGE_THUNK_DATA
					itd = (PDWORD)wb;
					*itd = (DWORD)((DWORD)iibn - (DWORD)point - secHeader->PointerToRawData + secHeader->VirtualAddress);
					wb += sizeof(DWORD);

					*((PDWORD)wb) = 0x0;
					wb += 4;

					// move Import Directory and add user32.dll IIDs
					newDic = (PBYTE)wb;

					dataDic[1].VirtualAddress = (DWORD)((DWORD)newDic - (DWORD)point - secHeader->PointerToRawData + secHeader->VirtualAddress);
					printf("newDic: %#x\n", newDic);
					printf("point: %#x\n", point);
					printf("secHeader->PTRD: %#x\n", secHeader->PointerToRawData);
					printf("secHeader->V: %#x\n", secHeader->VirtualAddress);
					printf("dataDic[1].VirtualAddress = %#x\n", dataDic[1].VirtualAddress);

					DWORD j = 0;

					for (; j < (sizeIIDs - 20); j++) {
						*newDic = *oldDic;

						newDic++;
						oldDic++;
					}

					// add user32.dll IIDs
					dww = (PDWORD)newDic;
					for (int k = 0; k < 3; k++) {
						*dww = 0x0;
						dww++;
					}

					*dww = (DWORD)((DWORD)inDLL - (DWORD)point - secHeader->PointerToRawData + secHeader->VirtualAddress);
					dww++;
					RVAfunction = ((DWORD)itd - (DWORD)point - secHeader->PointerToRawData + secHeader->VirtualAddress);
					*dww = RVAfunction;
					dww++;
					newDic = (PBYTE)dww;

					printf("\n");
					printf("RVA MessageBoxA function IMAGE_IMPORT_BY_NAME: %#x\n", *itd);
					printf("Section: %s\n", secHeader->Name);
					printf("\n");

					// add Null Terminator
					for (; j < sizeIIDs; j++) {
						*newDic = *oldDic;

						newDic++;
						oldDic++;
					}

					if( (DWORD)newDic > (DWORD)((DWORD)point + secHeader->PointerToRawData + secHeader->Misc.VirtualSize))
						secHeader->Misc.VirtualSize = (DWORD)((DWORD)newDic - (DWORD)point - secHeader->PointerToRawData);

					break;

				}
				secHeader++;
			}

			if (newDic <= 0) {
				printf("Caves .idata is not enough!");
				ExitProcess(0);
			}

		}

		// ******************** adding string to section *******************
		secHeader = IMAGE_FIRST_SECTION(peHeader);

		// adding text to section
		char* inText = NULL;
		DWORD RVAinText;

		for (int i = 0; i< fileHeader->NumberOfSections; i++) {
			if (i > 0)
				secHeader++;

			if ((getNullSize(secHeader) > (14))) {
				DWORD sizeNull = getNullSize(secHeader);
				inText = (char*)((DWORD)point + secHeader->PointerToRawData + secHeader->SizeOfRawData - sizeNull);

				RVAinText = (DWORD)(secHeader->SizeOfRawData - sizeNull + secHeader->VirtualAddress + opHeader->ImageBase);
				printf("RVA text: %#x\tSection: %s\n", (DWORD)inText, (char*)(secHeader->Name));

				strcpy_s(inText, 20, text);

				if( secHeader->Misc.VirtualSize < secHeader->SizeOfRawData)
					secHeader->Misc.VirtualSize += 14;

				break;
			}
		}

		if (inText == NULL) {
			printf("Caves data is not enough!");
			ExitProcess(0);
		}
		// ********** find CODE section and adding code to PE file*****************
		secHeader = IMAGE_FIRST_SECTION(peHeader);

		for (int i = 0; i < fileHeader->NumberOfSections; i++) {
			if (i > 0)
				secHeader++;

			if (((secHeader->Characteristics & 0x20) == 0x20) && (getNullSize(secHeader) > 23))
				break;
		}

		if (getNullSize(secHeader) <= 23) {
			printf("Caves code is not enough!");
			ExitProcess(0);
		}

		// get address MessageBoxA
		//DWORD dwAddr = (DWORD)GetProcAddress(LoadLibrary(L'User32.dll'), "MessageBoxA");
		//	printf("%#x\n", dwAddr);

		// changes EP
		DWORD sizeNull = getNullSize(secHeader);

		DWORD oldEP = opHeader->AddressOfEntryPoint;

		DWORD newEP = secHeader->SizeOfRawData - sizeNull + secHeader->VirtualAddress;

		printf("EP: %#x ==> %#x\t", oldEP, newEP);
		printf("Section: %s\n", secHeader->Name);
		printf("RVA function MessageBoxA: %#x\n", RVAfunction);

		opHeader->AddressOfEntryPoint = newEP;

		// inject code to file
		PBYTE pbCode = (PBYTE)((DWORD)point + secHeader->PointerToRawData + secHeader->SizeOfRawData - sizeNull);

		/*
		push 0
		push 0
		push inText
		push 0
		call MessageBoxA
		mov eax, oldEP
		jump eax
		*/
		int i = 0;
		for (; i < 5; i++) {
			*pbCode = codeASM[i];
			pbCode++;
		}

		// inText
		PDWORD pdwCode = (PDWORD)pbCode;
		*pdwCode = RVAinText;
		pdwCode++;
		pbCode = (PBYTE)pdwCode;

		for (; i < 9; i++) {
			*pbCode = codeASM[i];
			pbCode++;
		}


		RVAfunction += opHeader->ImageBase;
		// MessageBoxA
		pdwCode = (PDWORD)pbCode;
		*pdwCode = RVAfunction;
		pdwCode++;
		pbCode = (PBYTE)pdwCode;

		for (; i< 10; i++) {
			*pbCode = codeASM[i];
			pbCode++;
		}

		// oldEP
		pdwCode = (PDWORD)pbCode;
		*pdwCode = (DWORD)(oldEP + opHeader->ImageBase);
		pdwCode++;
		pbCode = (PBYTE)pdwCode;

		for (; i <12; i++) {
			*pbCode = codeASM[i];
			pbCode++;
		}

		if ((DWORD)pbCode > (DWORD)((DWORD)point + secHeader->PointerToRawData + secHeader->Misc.VirtualSize))
			secHeader->Misc.VirtualSize = (DWORD)((DWORD)pbCode - (DWORD)point - secHeader->PointerToRawData);
	}
}