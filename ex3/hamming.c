#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX 100

int code[MAX];
int filled[MAX];
char label[MAX][4];
char txBuffer[MAX] = "";
int txBufferLen = 0;

int isPowerOf2(int pos)
{
    return pos > 0 && (pos & (pos - 1)) == 0;
}

int findParityBits(int n)
{
    int p = 1;
    while (n + p + 1 > (int)pow(2, p))
    {
        p++;
    }
    return p;
}

void assignLabels(int totalLen, int p)
{
    int n = totalLen - p;
    int dNum = n;
    for (int j = totalLen; j >= 1; j--)
    {
        if (isPowerOf2(j))
        {
            int k = (int)(log2(j)) + 1;
            sprintf(label[j], "P%d", k);
        }
        else
        {
            sprintf(label[j], "D%d", dNum);
            dNum--;
        }
    }
}

void placeDataBits(const char *data, int totalLen)
{
    int idx = 0;
    for (int j = totalLen; j >= 1; j--)
    {
        if (!isPowerOf2(j))
        {
            code[j] = data[idx] - '0';
            filled[j] = 1;
            idx++;
        }
        else
        {
            code[j] = 0;
            filled[j] = 0;
        }
    }
}

void displayFrame(int totalLen)
{
    printf("\nPosition : ");
    for (int j = totalLen; j >= 1; j--)
        printf("%3d", j);

    printf("\nType     : ");
    for (int j = totalLen; j >= 1; j--)
        printf("%3s", label[j]);

    printf("\nValue    : ");
    for (int j = totalLen; j >= 1; j--)
    {
        if (filled[j])
            printf("%3d", code[j]);
        else
            printf("  _");
    }
    printf("\n");
}

void calculateParity(int totalLen, int p, int parityType)
{
    for (int k = 1; k <= p; k++)
    {
        int parityPos = 1 << (k - 1);
        if (parityPos > totalLen)
            continue;

        int count = 0;
        for (int j = 1; j <= totalLen; j++)
        {
            if (j & parityPos)
            {
                if (j != parityPos)
                {
                    count += code[j];
                }
            }
        }

        int bit;
        if (parityType == 0)
            bit = (count % 2 == 0) ? 0 : 1;
        else
            bit = (count % 2 == 0) ? 1 : 0;

        code[parityPos] = bit;
        filled[parityPos] = 1;
    }
}

void generateHammingCode(const char *data, int parityType)
{
    int n = strlen(data);
    int p = findParityBits(n);
    int totalLen = n + p;

    assignLabels(totalLen, p);
    placeDataBits(data, totalLen);
    calculateParity(totalLen, p, parityType);

    printf("\n--- Encoding Complete ---");
    displayFrame(totalLen);

    int idx = 0;
    for (int j = totalLen; j >= 1; j--)
    {
        txBuffer[idx++] = code[j] + '0';
    }
    txBuffer[idx] = '\0';
    txBufferLen = totalLen;

    printf("\nStored in Transmission Buffer: %s\n", txBuffer);
}

int detectError(int totalLen, int p, int parityType)
{
    int syndrome = 0;

    for (int k = p; k >= 1; k--)
    {
        int parityPos = 1 << (k - 1);
        if (parityPos > totalLen)
            continue;

        int count = 0;
        for (int j = 1; j <= totalLen; j++)
            if (j & parityPos)
                count += code[j];

        int checkBit;
        if (parityType == 0)
            checkBit = (count % 2 == 0) ? 0 : 1;
        else
            checkBit = (count % 2 == 0) ? 1 : 0;

        if (checkBit)
            syndrome += parityPos;
    }
    return syndrome;
}

void correctError(int errorPos, int totalLen)
{
    if (errorPos == 0)
    {
        printf("\nResult: No errors detected.\n");
    }
    else if (errorPos > totalLen)
    {
        printf("\nResult: Error position (%d) out of bounds! Multiple corruptions likely.\n", errorPos);
    }
    else
    {
        printf("\nResult: Error found at Position %d! Flipping bit to repair.\n", errorPos);
        code[errorPos] = code[errorPos] ? 0 : 1;

        printf("\n--- Corrected Frame ---");
        displayFrame(totalLen);
    }
}

void extractData(int totalLen)
{
    printf("\nExtracted Original Data: ");
    for (int j = totalLen; j >= 1; j--)
        if (!isPowerOf2(j))
            printf("%d", code[j]);
    printf("\n");
}

int main()
{
    int choice, parityType, p;
    char input[MAX];

    while (1)
    {
        printf("\n=============================\n");
        printf("=== HAMMING CODE PROGRAM ===\n");
        printf("=============================\n");
        printf("Current Buffer Status: %s\n", txBufferLen > 0 ? txBuffer : "[Empty]");
        printf("-----------------------------\n");
        printf("1. Encode new data\n");
        printf("2. Corrupt a bit in the buffer\n");
        printf("3. Process & decode buffer data\n");
        printf("4. Exit\n");
        printf("Enter choice: ");
        scanf("%d", &choice);

        if (choice == 4)
        {
            printf("\nExiting program. Goodbye!\n");
            break;
        }

        if (choice < 1 || choice > 4)
        {
            printf("\nInvalid choice. Please try again.\n");
            continue;
        }

        printf("Enter parity type (0 = Even, 1 = Odd): ");
        scanf("%d", &parityType);

        switch (choice)
        {
            case 1:
                printf("Enter data bits (e.g. 1011010): ");
                scanf("%s", input);
                generateHammingCode(input, parityType);
                break;

            case 2:
                if (txBufferLen == 0)
                {
                    printf("\nError: Transmission buffer is empty! Encode data first.\n");
                    break;
                }

                int corruptPos;
                printf("Enter bit position to corrupt (1 to %d, Right-to-Left): ", txBufferLen);
                scanf("%d", &corruptPos);

                if (corruptPos < 1 || corruptPos > txBufferLen)
                {
                    printf("Error: Position out of range!\n");
                }
                else
                {
                    int strIdx = txBufferLen - corruptPos;
                    txBuffer[strIdx] = (txBuffer[strIdx] == '0') ? '1' : '0';
                    printf("\nSuccess: Bit flipped! New buffer state: %s\n", txBuffer);
                }
                break;

            case 3:
                if (txBufferLen == 0)
                {
                    printf("\nError: Transmission buffer is empty! Encode data first.\n");
                    break;
                }

                p = 0;
                while ((int)pow(2, p) < txBufferLen + 1)
                    p++;

                assignLabels(txBufferLen, p);
                for (int j = txBufferLen, idx = 0; j >= 1; j--, idx++)
                {
                    code[j] = txBuffer[idx] - '0';
                    filled[j] = 1;
                }

                printf("\n--- Processing Receiver Data ---");
                displayFrame(txBufferLen);

                int syndrome = detectError(txBufferLen, p, parityType);
                printf("\nCalculated Syndrome = %d\n", syndrome);

                correctError(syndrome, txBufferLen);
                extractData(txBufferLen);
                break;
        }
    }
    return 0;
}
}
