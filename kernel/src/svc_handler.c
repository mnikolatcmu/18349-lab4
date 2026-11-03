/**
 * @file svc_handler.c
 *
 * @brief      
 *
 * @date     11/1/26  
 *
 * @author   Aravind Gunasekaran (agunasek)
 *         Julie Nikolatos (mnikolat)  
 */

#include <stdint.h>
#include <debug.h>

#define UNUSED __attribute__((unused))

void svc_c_handler( /*PASS WHATEVER ARGS YOU WANT*/ ) {
  int svc_number = -1;
  switch ( svc_number ) {
  default:
    DEBUG_PRINT( "Not implemented, svc num %d\n", svc_number );
    ASSERT( 0 );
  }
}