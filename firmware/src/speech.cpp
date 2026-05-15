// pesos firmware — speech composer
// Maps an integer peso total (0..999999) to a sequence of English clip IDs.
// Clip IDs map 1:1 to filenames in firmware/data/<id>.wav.

#include "speech.h"

// Index 0 is unused for DIGITS (we never speak "zero" mid-number).
static const char* DIGITS[10] = {
    "zero", "one", "two", "three", "four",
    "five", "six", "seven", "eight", "nine"
};

// Indexed by (n - 10): 10..19.
static const char* TEENS[10] = {
    "ten", "eleven", "twelve", "thirteen", "fourteen",
    "fifteen", "sixteen", "seventeen", "eighteen", "nineteen"
};

// Indexed by (n/10 - 2): 20, 30, ..., 90.
static const char* TENS[8] = {
    "twenty", "thirty", "forty", "fifty",
    "sixty", "seventy", "eighty", "ninety"
};

static void append_under_1000(uint32_t n, std::vector<const char*>& out) {
    uint32_t hundreds = n / 100;
    uint32_t tens_units = n % 100;

    if (hundreds > 0) {
        out.push_back(DIGITS[hundreds]);
        out.push_back("hundred");
    }
    if (tens_units == 0) return;

    if (tens_units < 10) {
        out.push_back(DIGITS[tens_units]);
    } else if (tens_units < 20) {
        out.push_back(TEENS[tens_units - 10]);
    } else {
        out.push_back(TENS[tens_units / 10 - 2]);
        if (tens_units % 10 > 0) {
            out.push_back(DIGITS[tens_units % 10]);
        }
    }
}

std::vector<const char*> compose_english(uint32_t pesos) {
    std::vector<const char*> result;

    // Clamp to 999999 — beyond that we'd need "million" clips and the count
    // can't physically reach there from 6 PHP denominations anyway. Without
    // this guard a caller passing 1234567 would index DIGITS[1234] OOB.
    if (pesos > 999999) {
        pesos = 999999;
    }

    if (pesos == 0) {
        result.push_back("zero");
        result.push_back("pesos");
        return result;
    }

    uint32_t thousands = pesos / 1000;
    uint32_t remainder = pesos % 1000;

    if (thousands > 0) {
        append_under_1000(thousands, result);
        result.push_back("thousand");
    }
    if (remainder > 0) {
        append_under_1000(remainder, result);
    }

    result.push_back(pesos == 1 ? "peso" : "pesos");
    return result;
}
