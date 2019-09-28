#pragma once

void reverse(char str[], int length) 
{ 
    int start = 0; 
    int end = length -1; 
    char c;

    while (start < end) 
    { 
        c = str[start];
        str[start] = str[end];
        str[end] = c;

        start++; 
        end--; 
    } 
} 
  
char* itoa(long num, char* str, int base) 
{ 
    long i = 0; 
    bool isNegative = false; 

    str[0] = 0;
    str[1] = 0;
  
    /* Handle 0 explicitely, otherwise empty string is printed for 0 */
    if (num == 0) 
    { 
        str[i++] = '0'; 
        str[i] = '\0'; 
        return str; 
    } 
  
    // In standard itoa(), negative numbers are handled only with  
    // base 10. Otherwise numbers are considered unsigned. 
    if (num < 0 && base == 10) 
    { 
        isNegative = true; 
        num = -num; 
    } 
  
    // Process individual digits 
    while (num != 0) 
    { 
        long rem = num % base; 
        str[i++] = (rem > 9) ? (rem-10) + 'A' : rem + '0'; 
        num = num/base; 
    } 
  
    // If number is negative, append '-' 
    if (isNegative) 
        str[i++] = '-'; 
  
    str[i] = '\0'; // Append string terminator 
  
    // Reverse the string 
    reverse(str, i); 
  
    return str; 
} 
  