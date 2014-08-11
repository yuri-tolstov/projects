/******************************************************************************/
/* File:   hbtool/n804.c                                                      */
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
/* N804 Definitions                                                           */
/******************************************************************************/
#define N804_SEG_NUM 2
#define N804_MDIO_BUS 0
#define N804_PHY 0x10

/******************************************************************************/
/* CPLD Definitions                                                           */
/******************************************************************************/
#define N804_CPLD_SIG_L  0x0
#define N804_CPLD_SIG_H  0x1
#define N804_CPLD_REV    0x2
/*AVR Intarface*/
#define N804_AVR_ADDR    0x10
#define N804_AVR_DOUT    0x11
#define N804_AVR_DIN     0x12
#define N804_AVR_CSR     0x17
#define N804_AVR_CSR_OP_REQ     (1 << 3)
#define N804_AVR_CSR_DIR_IN     (0 << 2)
#define N804_AVR_CSR_DIR_OUT    (1 << 2)
#define N804_AVR_CSR_KICK_A     (1 << 4)
#define N804_AVR_CSR_KICK_B     (1 << 5)

/******************************************************************************/
/* AVR Definitions                                                            */
/******************************************************************************/
/*The AVR Register addresses are used directly in the Interface functions below.*/

/******************************************************************************/
/* AVR access Functions                                                        */
/******************************************************************************/
static
uint8_t n804_avr_read(uint8_t avr_reg)
{
    cvmx_mdio_write(N804_MDIO_BUS, N804_PHY, N804_AVR_ADDR, avr_reg);
    cvmx_mdio_write(N804_MDIO_BUS, N804_PHY, N804_AVR_CSR, N804_AVR_CSR_OP_REQ | N804_AVR_CSR_DIR_IN);

    while (cvmx_mdio_read(N804_MDIO_BUS, N804_PHY, N804_AVR_CSR) & N804_AVR_CSR_OP_REQ)
        usleep(100);

    return (uint8_t)cvmx_mdio_read(N804_MDIO_BUS, N804_PHY, N804_AVR_DIN);
}

static
void n804_avr_write(uint8_t avr_reg, uint8_t data)
{
    cvmx_mdio_write(N804_MDIO_BUS, N804_PHY, N804_AVR_ADDR, avr_reg);
    cvmx_mdio_write(N804_MDIO_BUS, N804_PHY, N804_AVR_DOUT, data);
    cvmx_mdio_write(N804_MDIO_BUS, N804_PHY, N804_AVR_CSR, N804_AVR_CSR_OP_REQ | N804_AVR_CSR_DIR_OUT);

    while (cvmx_mdio_read(N804_MDIO_BUS, N804_PHY, N804_AVR_CSR) & N804_AVR_CSR_OP_REQ)
        usleep(100);
}

/******************************************************************************/
/* Niagara Interface Functions                                                */
/******************************************************************************/
int n804_avr_kick_heartbeat(int seg)
{
    static uint8_t seg_kicks[] = {N804_AVR_CSR_KICK_A | N804_AVR_CSR_KICK_B, N804_AVR_CSR_KICK_A, N804_AVR_CSR_KICK_B};
    cvmx_mdio_write(N804_MDIO_BUS, N804_PHY, N804_AVR_CSR, seg_kicks[seg]);
    return 0;
}

int n804_timeout_get(int seg)
{
    static uint8_t regs[] = {0x0, 0x1};
    uint8_t m = n804_avr_read(regs[seg - 1]);
    printf("Heartbeat timeout [%d]: %d msec\n", seg, (int) m * 100);
    return 0;
}

int n804_timeout_set(int seg, char *v)
{
    static uint8_t regs[] = {0x0, 0x1};
    int i = atoi(v); i /= 100;
    if (i <= 0xff) {
        n804_avr_write(regs[seg - 1], (uint8_t)i);
        return 0;
    }
    return -1;
}

int n804_startup_wait_get(int seg)
{
    uint8_t m = n804_avr_read(0x4);
    printf("Start up wait: %d sec\n", (int) m * 2);
    return 0;
}

int n804_startup_wait_set(int seg, char *v)
{
    int i = atoi(v); i /= 2;
    if (i <= 0xff) {
        n804_avr_write(0x4, (uint8_t)i);
        return 0;
    }
    return -1;
}

int n804_startup_wait_ovr_get(int seg)
{
    uint8_t m = n804_avr_read(0x5);
    printf("Start up wait override: %d\n", m);
    return 0;
}

int n804_startup_wait_ovr_set(int seg, char *v)
{
    uint8_t i = atoi(v);
    if (i < 2) {
        n804_avr_write(0x5, i);
        return 0;
    }
    return -1;
}

int n804_default_mode_get(int seg)
{
    static uint8_t regs[] = {0x32, 0x38};
    uint8_t m = n804_avr_read(regs[seg - 1]);
    printf("Default mode [%d]: %d\n", seg, (int) m);
    return 0;
}

int n804_default_mode_set(int seg, char *v)
{
    static uint8_t regs[] = {0x32, 0x38};
    uint8_t i = atoi(v);
    if (i < 6) {
        n804_avr_write(regs[seg - 1], i);
        return 0;
    }
    return -1;
}

int n804_poweroff_mode_get(int seg)
{
    static uint8_t regs[] = {0x33, 0x39};
    uint8_t m = n804_avr_read(regs[seg - 1]);
    printf("Power Off mode [%d]: %s\n", seg, (m)?"Bypass":"No Link");
    return 0;
}

int n804_poweroff_mode_set(int seg, char *v)
{
    static uint8_t regs[] = {0x33, 0x39};
    uint8_t i = atoi(v);
    if (i < 2) {
        n804_avr_write(regs[seg - 1], i);
        return 0;
    }
    return -1;
}

int n804_current_mode_get(int seg)
{
    static uint8_t regs[] = {0x31, 0x37};
    uint8_t m = n804_avr_read(regs[seg - 1]);
    printf("Current mode [%d]: %d\n", seg, (int) m);
    return 0;
}

int n804_current_mode_set(int seg, char *v)
{
    static uint8_t regs[] = {0x31, 0x37};
    uint8_t i = atoi(v);
    if (i < 6) {
        n804_avr_write(regs[seg - 1], i);
        return 0;
    }
    return -1;
}

int n804_status_get(int seg)
{
    static uint8_t regs[] = {0x2f, 0x35};
    static char *status[] = {"BYPASS", "INLINE", "NO LINK"};

    uint8_t s = n804_avr_read(regs[seg - 1]);
    printf("Status [%d]: %s\n", seg, status[s - 1]);
    return 0;
}

int n804_oem_id_get(int seg)
{
    uint8_t s = n804_avr_read(0x41);
    printf("OEM Id: %d\n", s);
    return 0;
}

int n804_product_rev_get(int seg)
{
    uint8_t s = n804_avr_read(0x43);
    printf("Product revision: %d\n", s);
    return 0;
}

int n804_product_id_get(int seg)
{
    uint8_t s = n804_avr_read(0x42);
    printf("Product Id: %d\n", s);
    return 0;
}

int n804_fw_rev_get(int seg)
{
    uint8_t s = n804_avr_read(0x44);
    uint8_t ss = n804_avr_read(0x45);
    printf("Firmware revision: %d.%d\n", s, ss);
    return 0;
}

int n804_numseg_get(void)
{
    return N804_SEG_NUM;
}

int n804_probe(void)
/* Returns: IMTHW_N804 or IMTHW_UNDEF.
 * The detection is peformed based on the Product ID AVR register.
 * Note, the save value can be found in CPLD ID register (address = 0).
 */
{
    /*Enable access to the MDIO bus.*/
    cvmx_write_csr(CVMX_SMIX_EN(N804_MDIO_BUS), 0x1);

    /*Retrieve the Product ID value.*/
    uint8_t s = n804_avr_read(0x42);
    printf("Product Id: %d\n", s); //TODO:debug
   //TODO:
   // 1. What value should I expect?
   // 2. 8- or 16-bits?

    return IMTHW_N804;
}

int n804_init(niagara_ops_t *ops)
{
    ops->probe = n804_probe;
    ops->numseg_get = n804_numseg_get;
    ops->timeout_get = n804_timeout_get;
    ops->timeout_set = n804_timeout_set;
    ops->startup_wait_get = n804_startup_wait_get;
    ops->startup_wait_set = n804_startup_wait_set;
    ops->startup_wait_ovr_get = n804_startup_wait_ovr_get;
    ops->startup_wait_ovr_set = n804_startup_wait_ovr_set;
    ops->default_mode_get = n804_default_mode_get;
    ops->default_mode_set = n804_default_mode_set;
    ops->poweroff_mode_get = n804_poweroff_mode_get;
    ops->poweroff_mode_set = n804_poweroff_mode_set;
    ops->current_mode_get = n804_current_mode_get;
    ops->current_mode_set = n804_current_mode_set;
    ops->status_get = n804_status_get;
    ops->oem_id_get = n804_oem_id_get;
    ops->product_rev_get = n804_product_rev_get;
    ops->product_id_get = n804_product_id_get;
    ops->fw_rev_get = n804_fw_rev_get;
    ops->kick_heartbeat = n804_avr_kick_heartbeat;
    return 0;
}

