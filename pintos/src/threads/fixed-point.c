#include "fixed-point.h"

Fixed fixed_add(Fixed a, Fixed b){
    return a + b;
}

Fixed fixed_sub(Fixed a, Fixed b){
    return a - b;
}

Fixed fixed_mul(Fixed a, Fixed b){
    return ((int64_t) a * b) / CORR;
}

Fixed fixed_div(Fixed a, Fixed b){
    return ((int64_t) a * CORR) / b;
}

Fixed itofixed(int a){
    return a * CORR;
}

int fixedtoi(Fixed a){
    if(a>=0){
        return (a + CORR/2) / CORR;
    }
    else{
        return (a - CORR/2) / CORR;
    }

}