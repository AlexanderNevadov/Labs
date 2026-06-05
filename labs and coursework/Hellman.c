#include <stdio.h>
#include <string.h>

#define SIZE 8

int convertBin(int number, int bin[])
{
    for (int i = SIZE - 1; i >= 0; i--)
    {
        bin[i] = number % 2;
        number /= 2;
    }
    return 0;
}

int convertDecimal(int bin[])
{
    int result = 0;
    for (int i = 0; i < SIZE; i++)
    {
        result = result * 2 + bin[i];
    }
    return result;
}

int modulInverse(int multiplier, int modul)
{
    for (int i = 1; i < modul; i++)
    {
        if ((multiplier * i) % modul == 1)
        {
            return i;
        }
    }
    return 0;
}

int main()
{
    int privateKey[SIZE] = { 2, 4, 7, 14, 28, 56, 112, 224 };
    int openKey[SIZE];
    int modul = 500;
    int multiplier = 23;

    int multiplierInverse = modulInverse(multiplier, modul);

    printf("Open key: ");
    for (int i = 0; i < SIZE; i++)
    {
        openKey[i] = (privateKey[i] * multiplier) % modul;
        printf("%d ", openKey[i]);
    }
    printf("\n");

    unsigned char message[1000] = { 0 };
    printf("Enter text: ");
    scanf("%999s", message);

    int messageLen = strlen(message);

    int cipher[1000] = { 0 };
    printf("Encrypted text: ");

    for (int i = 0; i < messageLen; i++)
    {
        int asciiValue = (int)message[i], cipherNum = 0;
        int bin[SIZE];
        convertBin(asciiValue, bin);

        for (int j = 0; j < SIZE; j++)
        {
            if (bin[j] == 1)
            {
                cipherNum += openKey[j];
            }
        }
        cipher[i] = cipherNum;
        printf("%d ", cipher[i]);
    }

    int restoredMessage[1000] = { 0 };
    printf("\nDecrypted text: ");

    for (int i = 0; i < messageLen; i++)
    {
        int currentNum = (cipher[i] * multiplierInverse) % modul;
        int decryptedValue = 0;
        int bin[SIZE] = { 0 };

        for (int j = SIZE - 1; j >= 0; j--)
        {
            if (currentNum >= privateKey[j])
            {
                currentNum -= privateKey[j];
                bin[j] = 1;
            }
        }
        int decimalValue = convertDecimal(bin);
        restoredMessage[i] = decimalValue;
    }

    for (int k = 0; k < messageLen; k++)
        printf("%c", restoredMessage[k]);
}
