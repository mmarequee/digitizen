/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */
 
inline __attribute__((__gnu_inline__)) void calculate_peak(float *buff, float *current, long int nframes)
{
    /* if(buff == NULL || current == NULL || nframes = 0) return 0; */
    __asm__ __volatile__(
        "pushq      $0x7FFFFFFF                     \n\t" /* xmm2 is the mask for fake ABS we're using                  */
        "movss      (%%rsp),                %%xmm2  \n\t"
        "addq       $8,                     %%rsp   \n\t"

        "movss      (%%rsi),                %%xmm0  \n\t" /* hold single value of current, don't pack yet               */
	
        "xorq       %%rbx,                  %%rbx   \n\t"

        /*              GCC supposedly forces 16-bit aligns on pointers, if this is true, leave out this part of code:  */

        "movq       %%rdi,                  %%rcx   \n\t" /* we need to align to & 0xF...F0 + 0x10 if not aligned       */
        "andq       $18446744073709551600,  %%rcx   \n\t" /* $0xFFFFFFFFFFFFFFF0                                        */
        "cmpq       %%rdi,                  %%rcx   \n\t" /* are we 16-bit aligned?                                     */
        "jz         .SIZE                           \n\t" 

        "addq       $0x10,                  %%rcx   \n\t"
        "subq       %%rdi,                  %%rcx   \n\t" /* rcx holds loop count for single float operations           */
        "shrq       $2,                     %%rcx   \n\t" /* /= sizeof(float)                                           */

        "cmpq       %%rcx,                  %2      \n\t" /* handle cases where buff is actually 1 single float         */
        "jle        .SIZE                           \n\t" 
	
        "subq       %%rcx,                  %2      \n\t" /* decrease loop count for aligned packed data                */

".PRE:                                              \n\t"	

        "movss      (%%rdi, %%rbx),         %%xmm1  \n\t" /* same as (%0, %%rcx), which doesn't work                    */  
        "andps      %%xmm2,                 %%xmm1  \n\t"
        "maxss      %%xmm1,                 %%xmm0  \n\t"
        "addq       $0x04,                  %%rbx   \n\t"
        "decq       %%rcx                           \n\t"
        "jnz        .PRE                            \n\t" 

".SIZE:                                             \n\t"	
        
        "movq       %2,                     %%rcx   \n\t"
 
        "cmpq       $4,                     %2      \n\t" 
        "jl         .POST                           \n\t" /* handle cases nframes < sizeof(float) - unlikely            */

        /* note that %%rbx holds the index value for base pointer for aligned packed data                               */

".ALIGNED:                                          \n\t"	

        /* until here */
	
        "shufps     $0x00,  %%xmm2,         %%xmm2  \n\t" /* put the ABS mask into each of 4 packed floats              */
        "shufps     $0x00,  %%xmm0,         %%xmm0  \n\t" /* put the ABS mask into each of 4 packed floats              */
        /* we don't need to ABS current since first value will be 0 and each next current will already be ABSed         */
        "andq       $18446744073709551612,  %2      \n\t" /* make aligned loop count divisible by sizeof(float)         */
        "subq       %2,                     %%rcx   \n\t" /* store the remaining loop count                             */

".LOOP:                                             \n\t"

        "movaps     (%%rdi, %%rbx),         %%xmm1  \n\t" /* same as (%0, %%rcx), which doesn't work                    */  
        "andps      %%xmm2,                 %%xmm1  \n\t"
        "maxps      %%xmm1,                 %%xmm0  \n\t"

        "addq       $0x10,                  %%rbx   \n\t" 
        "subq       $4,                     %2      \n\t" 
        "jnz        .LOOP                           \n\t"

        "movaps     %%xmm0,                 %%xmm1  \n\t" /* handle the remaining packed 4 floats gracefully:           */
        "shufps     $0x4e,  %%xmm1,         %%xmm1  \n\t" /* shuffle left & right pairs (1234 => 3412)                  */
        "maxps      %%xmm1,                 %%xmm0  \n\t" /* maximums of the two pairs                                  */
        "movaps     %%xmm0,                 %%xmm1  \n\t"
        "shufps     $0xb1,  %%xmm1,         %%xmm1  \n\t" /* shuffle the floats inside the two pairs (1234 => 2143)     */
        "maxps      %%xmm1,                 %%xmm0  \n\t"

        "cmp        $0,                     %%rcx   \n\t"
        "jz         .END                            \n\t"

".POST:                                             \n\t"   /* handle cases where nframes % sizeof(float) > 0           */

        "movss      (%%rdi, %%rbx),         %%xmm1  \n\t"   /* same as (%0, %%rcx), which doesn't work                  */  
        "andps      %%xmm2,                 %%xmm1  \n\t"
        "maxss      %%xmm1,                 %%xmm0  \n\t"
        "addq       $0x04,                  %%rbx   \n\t"
        "decq       %%rcx                           \n\t"
        "jnz        .POST                           \n\t"  

".END:                                              \n\t"

        "movss      %%xmm0,                 (%%rsi) \n\t"	
        :
        :"D"        (buff), "S" (current), "r" (nframes)
        :"xmm0", "xmm1", "xmm2", "rcx", "rbx"
        );
}
