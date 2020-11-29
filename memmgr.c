#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wbuiltin-requires-header"
#pragma ide diagnostic ignored "cert-err34-c"


//
//  memmgr.c
//  memmgr
//
//  Created by William McCarthy on 17/11/20.
//  Copyright © 2020 William McCarthy. All rights reserved.
//


//Frame size of 2^8 bytes (256 bytes), so one page == one frame. This is not normally the
//case, and it won’t be the case in the second part of this simulation.

//Also, your program need only be concerned with reading logical addresses and translating them to their
//corresponding physical addresses. You don’t need to support writing to the logical address space.

//http://www.cplusplus.com/reference/cstdio/fseek/ leaving here for reference



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define FILE_ERROR 2
#define BUFLEN 256
#define FRAME_SIZE 256
char mainMemory[65536];
char mainMemoryFIFO[32768];
int pageQueue[128];
int queueHead = 0;
int queueTail = 0;
int translationLookasideBuffer[16][2];
int translationLookasideBufferCurrent = 0;
int tableOfPages[256];
int currentFrame = 0;
int pageFaultCountArray[5] = {0};
int pageFaultCountTwoArray[5] = {0};
int translationLookasideBufferHitCount[5] = {0};
int translationLookasideBufferTwo[5] = {0};
int countOne[5] = {0};
int countTwo[5] = {0};
void simulationOne();
//the rubric wanted another simulation :/
void simulationTwo();
FILE *binFile;

unsigned fifoGetFrame(unsigned logic_add, unsigned page, int *pageFaultCount, int *translationLookasideBufferCount);
unsigned getPage(unsigned x);
unsigned getOffset(unsigned x);
unsigned getFrame(unsigned logic_add, unsigned page, int *pageFaultCount);
int findInTranslationLookasideBuffer(unsigned x);
int availableFrame(unsigned page);
void simulationOneResults();
void simulationTwoResults();

int main() {
    simulationOne();
    printf("\nSimulationOne Stats:\n");
    simulationOneResults();
    simulationTwo();
    printf("%s", "\n");
    printf("\nSimulationTwo Stats:\n");
    simulationTwoResults();
    return 0;
}

void simulationTwoResults() {
    printf("Number of Translated Addresses = %d\n", countTwo[4]);
    printf("Page Faults = %d\n", pageFaultCountTwoArray[4]);
    printf("Page Fault Rate = %.3f\n", 1.0f * pageFaultCountTwoArray[4] / countTwo[4]);
    printf("TLB Hits = %d\n", translationLookasideBufferTwo[4]);
    printf("TLB Hit Rate = %.3f\n", 1.0f * translationLookasideBufferTwo[4] / countTwo[4]);
}

void simulationOneResults() {
    printf("Number of Translated Addresses = %d\n", countOne[4]);
    printf("Page Faults = %d\n", pageFaultCountArray[4]);
    printf("Page Fault Rate = %.3f\n", 1.0f * pageFaultCountArray[4] / countOne[4]);
    printf("TLB Hits = %d\n", translationLookasideBufferHitCount[4]);
    printf("TLB Hit Rate = %.3f\n", 1.0f * translationLookasideBufferHitCount[4] / countOne[4]);
}


void simulationOne() {
    FILE *addressesFile = fopen("addresses.txt", "r");
    if (addressesFile == NULL) {
        fprintf(stderr, "address.txt not found \n");
        exit(FILE_ERROR);
    }
    FILE *correctFile = fopen("correct.txt", "r");
    if (correctFile == NULL) {
        fprintf(stderr, "correct.txt not found \n");
        exit(FILE_ERROR);
    }
    char buffer[BUFLEN];
    unsigned page;
    unsigned offset;
    unsigned physicalAddressNew;
    unsigned frame = 0;
    unsigned logic_add;
    unsigned virtualAddress, physicalAddress, value;
    binFile = fopen("BACKING_STORE.bin", "rb");
    if (binFile == NULL) {
        fprintf(stderr, "BACKING_STORE.bin not found \n");
        exit(FILE_ERROR);
    }
    for (int i = 0; i < 256; ++i) {
        tableOfPages[i] = -1;
    }
    for (int i = 0; i < 16; ++i) {
        translationLookasideBuffer[i][0] = -1;
    }
    int accessCount = 0, pageFaultCount = 0;
    int translationLookasideBufferCount = 0;
    currentFrame = 0;
    translationLookasideBufferCurrent = 0;
    while (fscanf(addressesFile, "%d", &logic_add) != EOF) {
        ++accessCount;
        fscanf(correctFile, "%s %s %d %s %s %d %s %d", buffer, buffer, &virtualAddress, buffer, buffer, &physicalAddress, buffer, &value);
        page = getPage(logic_add);
        offset = getOffset(logic_add);
        frame = getFrame(logic_add, page, &pageFaultCount);
        physicalAddressNew = frame * FRAME_SIZE + offset;
        int val = (int) (mainMemory[physicalAddressNew]);
        fseek(binFile, logic_add, 0);
        if (accessCount > 0 && accessCount % 200 == 0) {
            translationLookasideBufferHitCount[(accessCount / 200) - 1] = translationLookasideBufferCount;
            pageFaultCountArray[(accessCount / 200) - 1] = pageFaultCount;
            countOne[(accessCount / 200) - 1] = accessCount;
        }
        printf("logical: %5u (page: %3u, offset: %3u) ---> physical: %5u -- passed\n",logic_add, page, offset, physicalAddressNew);
        assert(physicalAddressNew == physicalAddress);
        assert(value == val);
    }
    fclose(addressesFile);
    fclose(binFile);
    fclose(correctFile);
    printf("ALL logical ---> physical assertions PASSED!\n");
    printf("ALL read memory value assertions PASSED!\n");
}

void simulationTwo() {
    char buffer[BUFLEN];
    unsigned page;
    unsigned physAddress;
    unsigned offset;
    unsigned logicalAddress;
    unsigned frame;
    unsigned physicalAddress;
    unsigned value;
    unsigned virtualAddress;

    for (int i = 0; i < 256; ++i) {
        tableOfPages[i] = -1;
    }
    for (int i = 0; i < 16; ++i) {
        translationLookasideBuffer[i][0] = -1;
    }
    for (int i = 0; i < 128; ++i) {
        pageQueue[i] = -1;
    }

    int translationLookasideBufferCount = 0;
    int accessCount = 0;
    int pageFaultCount = 0;
    queueTail = 0;
    queueHead = 0;

    FILE *addressesFile = fopen("addresses.txt", "r");
    if (addressesFile == NULL) {
        fprintf(stderr, "address.txt not found \n");
        exit(FILE_ERROR);
    }

    FILE *correctFile = fopen("correct.txt", "r");
    if (correctFile == NULL) {
        fprintf(stderr, "Could not open file: 'correct.txt'\n");
        exit(FILE_ERROR);
    }

    binFile = fopen("BACKING_STORE.bin", "rb");
    if (binFile == NULL) {
        fprintf(stderr, "BACKING_STORE.bin not found \n");
        exit(FILE_ERROR);
    }

    while (fscanf(addressesFile, "%d", &logicalAddress) != EOF) {
        ++accessCount;
        fscanf(correctFile, "%s %s %d %s %s %d %s %d", buffer, buffer, &virtualAddress, buffer, buffer, &physicalAddress, buffer, &value);
        page = getPage(logicalAddress);
        offset = getOffset(logicalAddress);
        frame = fifoGetFrame(logicalAddress, page, &pageFaultCount, &translationLookasideBufferCount);
        physAddress = frame * FRAME_SIZE + offset;
        int val = (int) (mainMemoryFIFO[physAddress]);
        fseek(binFile, logicalAddress, 0);
        if (accessCount > 0 && accessCount % 200 == 0) {
            translationLookasideBufferTwo[(accessCount / 200) - 1] = translationLookasideBufferCount;
            pageFaultCountTwoArray[(accessCount / 200) - 1] = pageFaultCount;
            countTwo[(accessCount / 200) - 1] = accessCount;
        }
        printf("logical: %5u (page: %3u, offset: %3u) ---> physical: %5u -- passed\n", logicalAddress, page, offset, physAddress);
        assert(value == val);
    }
    fclose(addressesFile);
    fclose(binFile);
    fclose(correctFile);
    printf("ALL read memory value assertions PASSED!\n");
}

unsigned getOffset(unsigned x) {
    return (0xff & x);
}

unsigned getPage(unsigned x) {
    return (0xff00 & x) >> 8;
}

int findInTranslationLookasideBuffer(unsigned x) {
    for (int i = 0; i < 16; i++) {
        if (translationLookasideBuffer[i][0] == x) {
            return i;
        }
    }
    return -1;
}


void updateTranslationLookasideBuffer(unsigned page) {
    translationLookasideBuffer[translationLookasideBufferCurrent][1] = tableOfPages[page];
    translationLookasideBuffer[translationLookasideBufferCurrent][0] = page;
    translationLookasideBufferCurrent = (translationLookasideBufferCurrent + 1) % 16;
}

unsigned getFrame(unsigned logic_add, unsigned page, int *pageFaultCount) {
    int tlb_index = findInTranslationLookasideBuffer(page);
    if (tlb_index != -1) {
        if (pageQueue[translationLookasideBuffer[tlb_index][1]] == page) {
            (*translationLookasideBufferHitCount)++;
            return translationLookasideBuffer[tlb_index][1];
        }
    }

    if (tableOfPages[page] != -1) {
        updateTranslationLookasideBuffer(page);
        return tableOfPages[page];
    }
    int offset = (logic_add / FRAME_SIZE) * FRAME_SIZE;
    fseek(binFile, offset, 0);
    tableOfPages[page] = currentFrame;
    currentFrame = (currentFrame + 1) % 256;
    (*pageFaultCount)++;
    fread(&mainMemory[tableOfPages[page] * FRAME_SIZE], sizeof(char), 256, binFile);
    updateTranslationLookasideBuffer(page);
    return tableOfPages[page];
}

unsigned fifoGetFrame(unsigned logic_add, unsigned page, int *pageFaultCount, int *translationLookasideBufferCount) {
    int translationLookasideBufferIndex = findInTranslationLookasideBuffer(page);
    if (translationLookasideBufferIndex != -1) {
        (*translationLookasideBufferCount)++;
        return translationLookasideBuffer[translationLookasideBufferIndex][1];
    }

    if (tableOfPages[page] != -1 && pageQueue[tableOfPages[page]] == page) {
        updateTranslationLookasideBuffer(page);
        return tableOfPages[page];
    }

    // finding the location in backing_store
    int offset = (logic_add / FRAME_SIZE) * FRAME_SIZE;
    fseek(binFile, offset, 0);
    int available_frame = availableFrame(page);
    fread(&mainMemoryFIFO[available_frame * FRAME_SIZE], sizeof(char), 256, binFile);
    tableOfPages[page] = available_frame;
    (*pageFaultCount)++;
    updateTranslationLookasideBuffer(page);
    return tableOfPages[page];
}

int availableFrame(unsigned page) {
    if (queueHead == 0 && queueTail == 0 && pageQueue[queueHead] == -1) {
        ++queueTail;
        pageQueue[queueHead] = page;
        return queueHead;
    }

    // Checking if the QUEUE ISN'T full
    if (pageQueue[queueTail] == -1) {
        pageQueue[queueTail] = page;
        int val = queueTail;
        queueTail = (queueTail + 1) % 128;
        return val;
    }

    // Checking if the QUEUE is full
    if (queueHead == queueTail && pageQueue[queueTail] != -1) {
        pageQueue[queueHead] = page;
        int ret = queueHead;
        queueHead = (queueHead + 1) % 128;
        queueTail = (queueTail + 1) % 128;
        return ret;
    }
}

#pragma clang diagnostic pop