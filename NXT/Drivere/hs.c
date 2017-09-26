/**
 * High Speed / RS485 Interface
 * This module provides basic access to the NXT RS485 hardware
 * Includes additional routines to perform lowlevel BitBus packet I/O
 * functions.
 * Author: Andy Shaw
 */
#include "hs.h"
#include  <string.h>
#include "display.h"
#include "FreeRTOS.h"
#include "sensors1.h"

// Buffer sizes etc,
// NOTE: The input code for this device assumes that 2 buffers are in use.
// Max data size
#define BUFSZ 64 // 10/04/16 takashic

// smaller than leJOS 10/04/16 takashic
#define IN_BUF_SZ  (BUFSZ)
#define OUT_BUF_SZ (BUFSZ)

#define IN_BUF_CNT 2
#define OUT_BUF_CNT 2
#define BAUD_RATE 460000

// statically allocated 10/04/15 takashic
//static unsigned char *in_buf[IN_BUF_CNT];
//static unsigned char *out_buf[OUT_BUF_CNT];
static unsigned char in_buf[IN_BUF_CNT][IN_BUF_SZ];
static unsigned char out_buf[OUT_BUF_CNT][OUT_BUF_SZ];

static unsigned char in_buf_in_ptr, out_buf_ptr;

static unsigned char* buf_ptr;

//static int in_buf_idx = 0;
static int in_buf_idx; // 10/04/16 takashic

	
//int hs_enable() // 10/04/15 takashic
int hs_enable(unsigned long baud_rate)
{
  unsigned char trash;
  // added divided by zero protection 10/04/15 takashic
  if (baud_rate == 0)
  {
	baud_rate = BAUD_RATE;
  }

  // Flush buffers 10/04/17 takashic
   memset(in_buf, 0, sizeof(in_buf));
   memset(out_buf, 0, sizeof(out_buf));

  // Initialize the device
  in_buf_in_ptr = out_buf_ptr = 0; 
  in_buf_idx = 0;
  
  // Enable power to the device
  *AT91C_PMC_PCER = (1 << AT91C_ID_US0); 
  
  // Disable pull ups
  *AT91C_PIOA_PPUDR = HS_RX_PIN | HS_TX_PIN | HS_RTS_PIN; 
  // Disable PIO A on I/O lines */
  *AT91C_PIOA_PDR = HS_RX_PIN | HS_TX_PIN | HS_RTS_PIN; 
  // Enable device control
  *AT91C_PIOA_ASR = HS_RX_PIN | HS_TX_PIN | HS_RTS_PIN; 
  // Now program up the device
  *AT91C_US0_CR   = AT91C_US_RSTSTA;
  *AT91C_US0_CR   = AT91C_US_STTTO;
  *AT91C_US0_RTOR = 0;
  *AT91C_US0_TTGR = 0;
  *AT91C_US0_IDR  = AT91C_US_TIMEOUT;
  *AT91C_US0_MR = AT91C_US_USMODE_RS485;
  *AT91C_US0_MR &= ~AT91C_US_SYNC;
  *AT91C_US0_MR |= AT91C_US_CLKS_CLOCK | AT91C_US_CHRL_8_BITS | AT91C_US_PAR_NONE | AT91C_US_NBSTOP_1_BIT | AT91C_US_OVER;
  *AT91C_US0_BRGR = ((configCPU_CLOCK_HZ/8/baud_rate) | (((configCPU_CLOCK_HZ/8) - ((configCPU_CLOCK_HZ/8/baud_rate) * baud_rate)) / ((baud_rate + 4)/8)) << 16);
  *AT91C_US0_PTCR = (AT91C_PDC_RXTDIS | AT91C_PDC_TXTDIS); 
  *AT91C_US0_RCR  = 0; 
  *AT91C_US0_TCR  = 0; 
  *AT91C_US0_RNPR = 0;
  *AT91C_US0_TNPR = 0;
  
  trash = *AT91C_US0_RHR;
  trash = *AT91C_US0_CSR;
  
  *AT91C_US0_RPR  = (unsigned int)&(in_buf[0][0]); 
  *AT91C_US0_RCR  = IN_BUF_SZ;
  *AT91C_US0_RNPR = (unsigned int)&(in_buf[1][0]);
  *AT91C_US0_RNCR = IN_BUF_SZ;

  *AT91C_US0_IER = 1;
  *AT91C_US0_CR   = AT91C_US_RXEN | AT91C_US_TXEN; 
  *AT91C_US0_PTCR = (AT91C_PDC_RXTEN | AT91C_PDC_TXTEN); 
  
  buf_ptr = &(in_buf[0][0]);
  
  return 1;
}

void hs_disable(void)
{
  // Turn off the device and make the pins available for other uses
  *AT91C_PMC_PCDR = (1 << AT91C_ID_US0);
  
  sp_reset(RS485_PORT);

}

void hs_init(void)
{
  // Initial state is off

  // flush buffers 10/04/16 takashic
  memset(in_buf, 0, sizeof(in_buf));
  memset(out_buf, 0, sizeof(out_buf));

  hs_disable();
}




unsigned long hs_write(unsigned char *buf, unsigned long off, unsigned long len)
{
  // Write data to the device. Return the number of bytes written
  if (*AT91C_US0_TNCR == 0)
  {	
    if (len > OUT_BUF_SZ) len = OUT_BUF_SZ;	
    memcpy(&(out_buf[out_buf_ptr][0]), buf+off, len);
    *AT91C_US0_TNPR = (unsigned int) &(out_buf[out_buf_ptr][0]);
    *AT91C_US0_TNCR = len;
    out_buf_ptr = (out_buf_ptr+1) % OUT_BUF_CNT;
    return len;
  }
  else
    return 0;
}

unsigned long hs_pending()
{
  // return the state of any pending i/o requests one bit for input one bit
  // for output.
  // First check for any input
  int ret = 0;
  int bytes_ready;
  if (*AT91C_US0_RNCR == 0) 
    bytes_ready = IN_BUF_SZ*2 - *AT91C_US0_RCR;
  else 
    bytes_ready = IN_BUF_SZ - *AT91C_US0_RCR;
  if (bytes_ready  > in_buf_idx) ret |= 1;
  if ((*AT91C_US0_TCR != 0) || (*AT91C_US0_TNCR != 0)) ret |= 2;
  return ret;
}



unsigned long hs_read(unsigned char * buf, unsigned long off, unsigned long len)
{
  int bytes_ready;
  int total_bytes_ready;
  int cmd_len;
  int i;
  unsigned char* tmp_ptr;
  
  cmd_len = 0;
  if (*AT91C_US0_RNCR == 0) {
    bytes_ready = IN_BUF_SZ;
    total_bytes_ready = IN_BUF_SZ*2 - *AT91C_US0_RCR;
  }
  else
    total_bytes_ready = bytes_ready = IN_BUF_SZ - *AT91C_US0_RCR;
  
  if (total_bytes_ready > in_buf_idx)
  {
    cmd_len = (int) (total_bytes_ready - in_buf_idx);
    if (cmd_len > len) cmd_len = len;
  	
    if (bytes_ready >= in_buf_idx + cmd_len)
    { 	
      for(i=0;i<cmd_len;i++) buf[off+i] = buf_ptr[in_buf_idx++];
    }
    else
    {
      for(i=0;i<cmd_len && in_buf_idx < IN_BUF_SZ;i++) buf[off+i] = buf_ptr[in_buf_idx++];
      in_buf_idx = 0;
      tmp_ptr = &(in_buf[(in_buf_in_ptr+1)%2][0]);
      for(;i<cmd_len;i++) buf[off+i] = tmp_ptr[in_buf_idx++];
      in_buf_idx += IN_BUF_SZ;
    } 
  }
  
  // Current buffer full and fully processed
  
  if (in_buf_idx >= IN_BUF_SZ && *AT91C_US0_RNCR == 0)
  { 	
    // Switch current buffer, and set up next 
    in_buf_idx -= IN_BUF_SZ;
    *AT91C_US0_RNPR = (unsigned int) buf_ptr;
    *AT91C_US0_RNCR = IN_BUF_SZ;
    in_buf_in_ptr = (in_buf_in_ptr+1) % IN_BUF_CNT;
    buf_ptr = &(in_buf[in_buf_in_ptr][0]);
  }
  return cmd_len;   
}


unsigned long hs_read_delimiter(unsigned char * buf, unsigned long off, unsigned long len, unsigned char delimiter)
{
  int bytes_ready;
  int total_bytes_ready;
  int cmd_len;
  int i;
  int delimiter_pos = -1;
  unsigned char* tmp_ptr;
  int in_buf_idx_old = in_buf_idx;
  cmd_len = 0;
  if (*AT91C_US0_RNCR == 0) {
    bytes_ready = IN_BUF_SZ;
    total_bytes_ready = IN_BUF_SZ*2 - *AT91C_US0_RCR;
  }
  else
    total_bytes_ready = bytes_ready = IN_BUF_SZ - *AT91C_US0_RCR;
  
  if (total_bytes_ready > in_buf_idx)
  {
    cmd_len = (int) (total_bytes_ready - in_buf_idx);
    if (cmd_len > len) cmd_len = len;
  	
    if (bytes_ready >= in_buf_idx + cmd_len)
    { 	
      for(i=0;i<cmd_len;i++) {
        buf[off+i] = buf_ptr[in_buf_idx++];
        if(buf[off+i] == delimiter) {
          delimiter_pos = i;
          break;
        }
      }
    }
    else
    {
      for(i=0;i<cmd_len && in_buf_idx < IN_BUF_SZ;i++) {
        buf[off+i] = buf_ptr[in_buf_idx++];
        if(buf[off+i] == delimiter) {
          delimiter_pos = i;
          break;
        }
      }
      if(delimiter_pos == -1) { 
        in_buf_idx = 0;
        tmp_ptr = &(in_buf[(in_buf_in_ptr+1)%2][0]);
        for(;i<cmd_len;i++) {
          buf[off+i] = tmp_ptr[in_buf_idx++];
          if(buf[off+i] == delimiter) {
            delimiter_pos = i;
            break;
          }
        }
        in_buf_idx += IN_BUF_SZ;
      }
    } 
  }
  
  if(delimiter_pos == -1) in_buf_idx = in_buf_idx_old;
  else if (in_buf_idx >= IN_BUF_SZ && *AT91C_US0_RNCR == 0)
  { 	
    // Switch current buffer, and set up next 
    in_buf_idx -= IN_BUF_SZ;
    *AT91C_US0_RNPR = (unsigned int) buf_ptr;
    *AT91C_US0_RNCR = IN_BUF_SZ;
    in_buf_in_ptr = (in_buf_in_ptr+1) % IN_BUF_CNT;
    buf_ptr = &(in_buf[in_buf_in_ptr][0]);
  }

  return delimiter_pos+1;   
}
