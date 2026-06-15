/*
This file is part of Spindle.  For copyright information see the COPYRIGHT
file in the top level directory, or at
https://github.com/hpc/Spindle/blob/master/COPYRIGHT

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License (as published by the Free Software
Foundation) version 2.1 dated February 1999.  This program is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED
WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms
and conditions of the GNU Lesser General Public License for more details.  You should
have received a copy of the GNU Lesser General Public License along with this
program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "crash_functions.h"

__attribute__((noinline, visibility("default")))
void crash_function_A(int rank) {
    volatile int *p = (int *) 0;
    (void) rank;
    *p = 0;
}

__attribute__((noinline, visibility("default")))
void crash_function_B(int rank) {
    volatile int *p = (int *) 0;
    (void) rank;
    *p = 0;
}

#define DEFINE_CRASH_N(n) \
    __attribute__((noinline, visibility("default"))) \
    void crash_function_##n(int rank) { \
        volatile int *p = (int *) 0; \
        (void) rank; \
        *p = 0; \
    }

DEFINE_CRASH_N(0)  DEFINE_CRASH_N(1)  DEFINE_CRASH_N(2)  DEFINE_CRASH_N(3)
DEFINE_CRASH_N(4)  DEFINE_CRASH_N(5)  DEFINE_CRASH_N(6)  DEFINE_CRASH_N(7)
DEFINE_CRASH_N(8)  DEFINE_CRASH_N(9)  DEFINE_CRASH_N(10) DEFINE_CRASH_N(11)
DEFINE_CRASH_N(12) DEFINE_CRASH_N(13) DEFINE_CRASH_N(14) DEFINE_CRASH_N(15)
DEFINE_CRASH_N(16) DEFINE_CRASH_N(17) DEFINE_CRASH_N(18) DEFINE_CRASH_N(19)
DEFINE_CRASH_N(20) DEFINE_CRASH_N(21) DEFINE_CRASH_N(22) DEFINE_CRASH_N(23)
DEFINE_CRASH_N(24) DEFINE_CRASH_N(25) DEFINE_CRASH_N(26) DEFINE_CRASH_N(27)
DEFINE_CRASH_N(28) DEFINE_CRASH_N(29) DEFINE_CRASH_N(30) DEFINE_CRASH_N(31)
DEFINE_CRASH_N(32) DEFINE_CRASH_N(33) DEFINE_CRASH_N(34) DEFINE_CRASH_N(35)
DEFINE_CRASH_N(36) DEFINE_CRASH_N(37) DEFINE_CRASH_N(38) DEFINE_CRASH_N(39)
DEFINE_CRASH_N(40) DEFINE_CRASH_N(41) DEFINE_CRASH_N(42) DEFINE_CRASH_N(43)
DEFINE_CRASH_N(44) DEFINE_CRASH_N(45) DEFINE_CRASH_N(46) DEFINE_CRASH_N(47)
DEFINE_CRASH_N(48) DEFINE_CRASH_N(49) DEFINE_CRASH_N(50) DEFINE_CRASH_N(51)
DEFINE_CRASH_N(52) DEFINE_CRASH_N(53) DEFINE_CRASH_N(54) DEFINE_CRASH_N(55)
DEFINE_CRASH_N(56) DEFINE_CRASH_N(57) DEFINE_CRASH_N(58) DEFINE_CRASH_N(59)
DEFINE_CRASH_N(60) DEFINE_CRASH_N(61) DEFINE_CRASH_N(62) DEFINE_CRASH_N(63)

crash_fn_t crash_table[CRASH_TABLE_SIZE] = {
    crash_function_0,  crash_function_1,  crash_function_2,  crash_function_3,
    crash_function_4,  crash_function_5,  crash_function_6,  crash_function_7,
    crash_function_8,  crash_function_9,  crash_function_10, crash_function_11,
    crash_function_12, crash_function_13, crash_function_14, crash_function_15,
    crash_function_16, crash_function_17, crash_function_18, crash_function_19,
    crash_function_20, crash_function_21, crash_function_22, crash_function_23,
    crash_function_24, crash_function_25, crash_function_26, crash_function_27,
    crash_function_28, crash_function_29, crash_function_30, crash_function_31,
    crash_function_32, crash_function_33, crash_function_34, crash_function_35,
    crash_function_36, crash_function_37, crash_function_38, crash_function_39,
    crash_function_40, crash_function_41, crash_function_42, crash_function_43,
    crash_function_44, crash_function_45, crash_function_46, crash_function_47,
    crash_function_48, crash_function_49, crash_function_50, crash_function_51,
    crash_function_52, crash_function_53, crash_function_54, crash_function_55,
    crash_function_56, crash_function_57, crash_function_58, crash_function_59,
    crash_function_60, crash_function_61, crash_function_62, crash_function_63,
};

static __attribute__((noinline))
void static_crash_inner(int rank) {
    volatile int *p = (int *) 0;
    (void) rank;
    *p = 0;
}

void static_crash(int rank) {
    static_crash_inner(rank);
}
