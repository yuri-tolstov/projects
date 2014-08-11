/******************************************************************************/
/* File:   hbtool/n808.c                                                      */
/******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "cvmx-warn.h"
#include "cvmx.h"
#include "octeon-remote.h"
#include "oct-remote-common.h"
#include "cvmx-mio-defs.h"
#include "cvmx-platform.h"
#include "cvmx-clock.h"
#include "cvmx-mdio.h"

#include "niagara.h"

/******************************************************************************/
/* N808 Definitions                                                           */
/******************************************************************************/
#define N808_SEG_NUM 2

/******************************************************************************/
/* CPLD Definitions                                                           */
/******************************************************************************/

/******************************************************************************/
/* AVR Definitions                                                            */
/******************************************************************************/
#define N808_MDIO_BUS 0
#define N808_PHY 0x10
#define N808_AVR_ADDR 0x10
#define N808_AVR_CSR 0x17
#define N808_AVR_DIN 0x12
#define N808_AVR_DOUT 0x11
#define N808_AVR_CSR_OP_REQ (1 << 3)
#define N808_AVR_CSR_DIR_IN (0 << 2)
#define N808_AVR_CSR_DIR_OUT (1 << 2)
#define N808_AVR_CSR_KICK_A (1 << 4)
#define N808_AVR_CSR_KICK_B (1 << 5)

/******************************************************************************/
/* AVR access Functions                                                        */
/******************************************************************************/
static
uint8_t n808_avr_read(uint8_t avr_reg)
{
    cvmx_mdio_write(N808_MDIO_BUS, N808_PHY, N808_AVR_ADDR, avr_reg);
    cvmx_mdio_write(N808_MDIO_BUS, N808_PHY, N808_AVR_CSR, N808_AVR_CSR_OP_REQ | N808_AVR_CSR_DIR_IN);

    while (cvmx_mdio_read(N808_MDIO_BUS, N808_PHY, N808_AVR_CSR) & N808_AVR_CSR_OP_REQ)
        usleep(100);

    return (uint8_t)cvmx_mdio_read(N808_MDIO_BUS, N808_PHY, N808_AVR_DIN);
}

static
void n808_avr_write(uint8_t avr_reg, uint8_t data)
{
    cvmx_mdio_write(N808_MDIO_BUS, N808_PHY, N808_AVR_ADDR, avr_reg);
    cvmx_mdio_write(N808_MDIO_BUS, N808_PHY, N808_AVR_DOUT, data);
    cvmx_mdio_write(N808_MDIO_BUS, N808_PHY, N808_AVR_CSR, N808_AVR_CSR_OP_REQ | N808_AVR_CSR_DIR_OUT);

    while (cvmx_mdio_read(N808_MDIO_BUS, N808_PHY, N808_AVR_CSR) & N808_AVR_CSR_OP_REQ)
        usleep(100);
}

static
int n808_avr_kick_heartbeat(int seg)
{
    static uint8_t seg_kicks[] = {N808_AVR_CSR_KICK_A | N808_AVR_CSR_KICK_B, N808_AVR_CSR_KICK_A, N808_AVR_CSR_KICK_B};
    cvmx_mdio_write(N808_MDIO_BUS, N808_PHY, N808_AVR_CSR, seg_kicks[seg]);
    return 0;
}

/******************************************************************************/
/* Niagara Interface Functions                                                */
/******************************************************************************/
int n808_timeout_get(int seg)
{
    static uint8_t seg_regs[] = {0x0, 0x1};
    uint8_t m = n808_avr_read(seg_regs[seg - 1]);
    printf("Heartbeat timeout [%d]: %d msec\n", seg, (int) m * 100);
    return 0;
}

int n808_timeout_set(int seg, char *v)
{
    static uint8_t seg_regs[] = {0x0, 0x1};
    int i = atoi(v); i /= 100;
    if (i <= 0xff) {
        n808_avr_write(seg_regs[seg - 1], (uint8_t)i);
        return 0;
    }
    return -1;
}

int n808_startup_wait_get(int seg)
{
    uint8_t m = n808_avr_read(0x4);
    printf("Start up wait: %d sec\n", (int) m * 2);
    return 0;
}

int n808_startup_wait_set(int seg, char *v)
{
    int i = atoi(v); i /= 2;
    if (i <= 0xff) {
        n808_avr_write(0x4, (uint8_t)i);
        return 0;
    }
    return -1;
}

int n808_startup_wait_ovr_get(int seg)
{
    uint8_t m = n808_avr_read(0x5);
    printf("Start up wait override: %d\n", m);
    return 0;
}

int n808_startup_wait_ovr_set(int seg, char *v)
{
    uint8_t i = atoi(v);
    if (i < 2) {
        n808_avr_write(0x5, i);
        return 0;
    }
    return -1;
}

int n808_default_mode_get(int seg)
{
    static uint8_t seg_regs[] = {0x32, 0x38};
    uint8_t m = n808_avr_read(seg_regs[seg - 1]);
    printf("Default mode [%d]: %d\n", seg, (int) m);
    return 0;
}

int n808_default_mode_set(int seg, char *v)
{
    static uint8_t seg_regs[] = {0x32, 0x38};
    uint8_t i = atoi(v);
    if (i < 6) {
        n808_avr_write(seg_regs[seg - 1], i);
        return 0;
    }
    return -1;
}

int n808_poweroff_mode_get(int seg)
{
    static uint8_t seg_regs[] = {0x33, 0x39};
    uint8_t m = n808_avr_read(seg_regs[seg - 1]);
    printf("Power Off mode [%d]: %s\n", seg, (m)?"Bypass":"No Link");
    return 0;
}

int n808_poweroff_mode_set(int seg, char *v)
{
    static uint8_t seg_regs[] = {0x33, 0x39};
    uint8_t i = atoi(v);
    if (i < 2) {
        n808_avr_write(seg_regs[seg - 1], i);
        return 0;
    }
    return -1;
}

int n808_current_mode_get(int seg)
{
    static uint8_t seg_regs[] = {0x31, 0x37};
    uint8_t m = n808_avr_read(seg_regs[seg - 1]);
    printf("Current mode [%d]: %d\n", seg, (int) m);
    return 0;
}

int n808_current_mode_set(int seg, char *v)
{
    static uint8_t seg_regs[] = {0x31, 0x37};
    uint8_t i = atoi(v);
    if (i < 6) {
        n808_avr_write(seg_regs[seg - 1], i);
        return 0;
    }
    return -1;
}

int n808_status_get(int seg)
{
    static uint8_t seg_regs[] = {0x2f, 0x35};
    static char *status[] = {"BYPASS", "INLINE", "NO LINK"};

    uint8_t s = n808_avr_read(seg_regs[seg - 1]);
    printf("Status [%d]: %s\n", seg, status[s - 1]);
    return 0;
}

int n808_oem_id_get(int seg)
{
    uint8_t s = n808_avr_read(0x41);
    printf("OEM Id: %d\n", s);
    return 0;
}

int n808_product_rev_get(int seg)
{
    uint8_t s = n808_avr_read(0x43);
    printf("Product revision: %d\n", s);
    return 0;
}

int n808_product_id_get(int seg)
{
    uint8_t s = n808_avr_read(0x42);
    printf("Product Id: %d\n", s);
    return 0;
}

int n808_fw_rev_get(int seg)
{
    uint8_t s = n808_avr_read(0x44);
    uint8_t ss = n808_avr_read(0x45);
    printf("Firmware revision: %d.%d\n", s, ss);
    return 0;
}

int n808_numseg_get(void)
{
   return 0; //TODO
}

int n808_probe(void)
/*Returns: IMTHW_N808 or IMTHW_UNDEF*/
{
   //TODO
   cvmx_write_csr(CVMX_SMIX_EN(N808_MDIO_BUS), 0x1);
   return IMTHW_N808;
}

int n808_init(niagara_ops_t *ops)
{
   ops->probe = n808_probe;
   ops->numseg_get = n808_numseg_get;
   ops->timeout_get = n808_timeout_get;
   ops->timeout_set = n808_timeout_set;
   ops->startup_wait_get = n808_startup_wait_get;
   ops->startup_wait_set = n808_startup_wait_set;
   ops->startup_wait_ovr_get = n808_startup_wait_ovr_get;
   ops->startup_wait_ovr_set = n808_startup_wait_ovr_set;
   ops->default_mode_get = n808_default_mode_get;
   ops->default_mode_set = n808_default_mode_set;
   ops->poweroff_mode_get = n808_poweroff_mode_get;
   ops->poweroff_mode_set = n808_poweroff_mode_set;
   ops->current_mode_get = n808_current_mode_get;
   ops->current_mode_set = n808_current_mode_set;
   ops->status_get = n808_status_get;
   ops->oem_id_get = n808_oem_id_get;
   ops->product_rev_get = n808_product_rev_get;
   ops->product_id_get = n808_product_id_get;
   ops->fw_rev_get = n808_fw_rev_get;
   ops->kick_heartbeat = n808_avr_kick_heartbeat;
   return 0;
}

