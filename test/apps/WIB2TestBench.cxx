/**
 * @file WIB2TestBench.cxx  Example of expanding a WIB2 frame using AVX2 
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2022.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "detdataformats/wib2/WIB2Frame.hpp"

#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <algorithm>


#include <cstring>
#include <immintrin.h>
#include <cstdio> // For printf
#include <array>
#include <chrono>
#include <stdio.h>
#include <stdint.h>

// A little wrapper around an array of 256-bit registers, so that we
// can explicitly access it as an array of 256-bit registers or as an
// array of uint16_t
template<size_t N>
class RegisterArray
{
public:
    // Get the value at the ith position as a 256-bit register
    inline __m256i ymm(size_t i) const { return _mm256_lddqu_si256(reinterpret_cast<const __m256i*>(m_array)+i); }
    inline void set_ymm(size_t i, __m256i val) { _mm256_storeu_si256(reinterpret_cast<__m256i*>(m_array)+i, val); }

    inline uint16_t uint16(size_t i) const { return m_array[i]; }
    inline void set_uint16(size_t i, uint16_t val) { m_array[i]=val; }

    // Access the jth entry in the ith register
    inline uint16_t uint16(size_t i, size_t j) const { return m_array[16*i+j]; }
    inline void set_uint16(size_t i, size_t j, uint16_t val) { m_array[16*i+j]=val; }

    inline uint16_t* data() { return m_array; }
    inline const uint16_t* data() const { return m_array; }

    inline size_t size() const { return N; }
private:
    alignas(32) uint16_t __restrict__ m_array[N*16];
};

//==============================================================================
// Print a 256-bit register interpreting it as packed 8-bit values
void print256(__m256i var)
{
    uint8_t *val = (uint8_t*) &var;
    for(int i=0; i<32; ++i){
        printf("%02x ", val[i]);
        if(i==15) printf("|| ");
        else{
            if(i%2) printf("| ");
            else    printf(" ");
        }
    }
}

//==============================================================================
__m256i unpack_one_register( dunedaq::detdataformats::wib2::WIB2Frame::word_t* first_word)
{
    __m256i reg=_mm256_lddqu_si256((__m256i*)first_word);
    // printf("Input:      ");
    // print256(reg);
    // printf("\n");

    // The register initially contains 18-and-a-bit 14-bit ADCs, but
    // we only have space for 16 after expansion, so the last 32-bit
    // word is unused. Copy word 3 so it appears twice, and move the
    // later words down one
    __m256i idx=_mm256_set_epi32(6, 5, 4, 3, 3, 2, 1, 0);
    __m256i shuf1=_mm256_permutevar8x32_epi32(reg, idx);
    // printf("shuf1:      ");
    // print256(shuf1);
    // printf("\n");

    // Each 32-bit word contains at least one full 14-bit ADC. Shift
    // the words by variable amounts s.t. the high 16 bits of each
    // word contains a 14-bit ADC at the right place (with the two
    // high bits still needing to be masked to zero). That result is
    // in `high_half`
    // __mmask8 mask=0xffu;
    // __m256i src=_mm256_set1_epi32(0);
    // The amounts by which we shift each 32-bit word
    __m256i count1=_mm256_set_epi32(12, 8, 4, 0, 14, 10, 6, 2);
    // __m256i high_half=_mm256_mask_sllv_epi32(src, mask, shuf1, count1);
    __m256i high_half=_mm256_sllv_epi32(shuf1, count1);
    // Mask out the low 16 bits, and the high two bits in the high half
    __m256i high_half_mask=_mm256_set1_epi32(0x3fff0000u);

    high_half=_mm256_and_si256(high_half, high_half_mask);
    // high_half2=_mm256_and_si256(high_half2, high_half_mask);

    // printf("high_half:  ");
    // print256(high_half);
    // printf("\n");

    //------------------------------------------------------------------
    // Now we start the process of setting the low 16 bits of each
    // word to the right value. This is trickier because now the bits
    // are spread across two words. First, left-shift each word so
    // that the higher bits of the ADC are in the right place
    __m256i count2=_mm256_set_epi32(10, 6, 2, 0, 12, 8, 4, 0);
    __m256i shift2=_mm256_sllv_epi32(shuf1, count2);
    // printf("shift2:     ");
    // print256(shift2);
    // printf("\n");

    // Next, permute the register so that the words containing the low
    // bits of the ADCs we want are in the same positions as the words
    // containing the corresponding high bits. This just amounts to
    // moving the words down by one
    __m256i idx2=_mm256_set_epi32(5, 4, 3, 2, 2, 1, 0, 0);
    __m256i shuf2=_mm256_permutevar8x32_epi32(reg, idx2);
    // printf("shuf2:      ");
    // print256(shuf2);
    // printf("\n");

    // Shift each word right by the amount that brings those low bits
    // into the right place, putting the result in `shift3`
    __m256i count3=_mm256_set_epi32(22, 26, 30, 0, 20, 24, 28, 0);
    __m256i shift3=_mm256_srlv_epi32(shuf2, count3);
    // printf("shift3:     ");
    // print256(shift3);
    // printf("\n");

    // OR together the registers containing the high and low bits of
    // the ADCs. At this point, the low 16 bits of each word should
    // contain the 14 bits of the ADCs in the right place (with the
    // two high bits still needing to be masked out)
    __m256i low_half=_mm256_or_si256(shift2, shift3);
    // Mask out the high 16 bits, and the high two bits in the high half
    __m256i low_half_mask=_mm256_set1_epi32(0x3fffu);
    low_half=_mm256_and_si256(low_half, low_half_mask);
    // printf("low_half:   ");
    // print256(low_half);
    // printf("\n");

    // Nearly there... Now we OR together the low and high halves
    __m256i both=_mm256_or_si256(low_half, high_half);
    // zero out the slot where we want to put the 16th value
    both=_mm256_andnot_si256(_mm256_set_epi32(0, 0, 0, 0xffffu, 0, 0, 0, 0), both);
    // printf("both:       ");
    // print256(both);
    // printf("\n");

    // We just missed the 16th value, and the lw 16 bits of the 8th
    // word are available, so shuffle it around to put it there
    __m256i shift4=_mm256_srli_epi32(reg, 18);
    // Mask so that's the only nonzero thing
    shift4=_mm256_and_si256(_mm256_set_epi32(0, 0x3fffu, 0, 0, 0, 0, 0, 0), shift4);
    // Move the word containing the value we want into the position we want
    __m256i idx3=_mm256_set_epi32(0, 0, 0, 6, 0, 0, 0, 0);
    __m256i shuf3=_mm256_permutevar8x32_epi32(shift4, idx3);

    both=_mm256_or_si256(both, shuf3);
    // printf("both':      ");
    // print256(both);
    // printf("\n");

    return both;
}

RegisterArray<16> unpack( dunedaq::detdataformats::wib2::WIB2Frame& frame)
{
    RegisterArray<16> ret;
    for(size_t i=0; i<ret.size(); ++i){
        ret.set_ymm(i, unpack_one_register(frame.adc_words+7*i));
    }
    return ret;
}


bool in_out_test(const std::array<uint16_t, 256>& vals)
{
    dunedaq::detdataformats::wib2::WIB2Frame frame;
    // Zero it out first
    std::memset(&frame, 0, sizeof(dunedaq::detdataformats::wib2::WIB2Frame));

    for(size_t i=0; i<vals.size(); ++i){
        frame.set_adc(i, vals[i]);
    }

    bool success=true;
    for(size_t i=0; i<vals.size(); ++i){
        uint16_t out_val=frame.get_adc(i);
        uint16_t in_val=vals[i];
        if(out_val!=in_val){
            success=false;
            printf("Failure at index %zu. Input: %03x, output: %03x\n", i, in_val, out_val);
        }
    }
    return success;
}


int main()
{


    // Some preliminary testing when creating an input array 	
    {
        printf("Test 1:\n");
        std::array<uint16_t, 256> vals;
        for(size_t i=0; i<vals.size(); ++i) vals[i]=i;
        in_out_test(vals);
    }

    {
        printf("Test 2:\n");
        std::array<uint16_t, 256> vals;
        for(size_t i=0; i<vals.size(); ++i) vals[i]=0x3a0+i;
        in_out_test(vals);
    }
      

    
    // Test WIB unpacking

    dunedaq::detdataformats::wib2::WIB2Frame frame;

    // Zero it out first
    std::memset(&frame, 0, sizeof(dunedaq::detdataformats::wib2::WIB2Frame));

    for(int i=0; i<256; ++i){
        frame.set_adc(i, 0x3a0+i);
    }

    std::array<int, 16> indices{0, 1, 2, 3, 4, 5, 6, 7, 15, 8, 9, 10, 11, 12, 13, 14};
    RegisterArray<16> unpacked=unpack(frame);

    //print256(unpacked);
    //printf("\n");

    bool success=true;
    for(int i=0; i<256; ++i){
        int in_index=16*(i/16)+indices[i%16];
        uint16_t in_val=frame.get_adc(in_index);
	std::cout << in_val << std::endl;
        uint16_t out_val=unpacked.uint16(i);
        // printf("%03d %03d %03d\n", in_index, in_val, out_val);
        if(in_val!=out_val){
            success=false;
        }
    }
    printf("Success? %d\n", success);


    std::cout << "Finished" << std::endl;




}

