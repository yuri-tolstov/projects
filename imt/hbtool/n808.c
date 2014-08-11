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
#define N808_MDIO_BUS 0
#define N808_PHY 0x10

/*Bypass segments.*/
#define N808_SEG_NUM 2
enum {
   N808_SEG_1,
   N808_SEG_2,
};

/******************************************************************************/
/* CPLD Definitions                                                           */
/******************************************************************************/
#define N808_CPLD_SIG_L  0x0
#define N808_CPLD_SIG_H  0x1
#define N808_CPLD_REV    0x2
/*AVR Interface, Port 1*/
#define N808_CSR_1       0x48
#define N808_ADDR_1      0x49
#define N808_DAT_1       0x4A
#define N808_SYS_CSR     0x4E
#define N808_SI_CTL      0x4F
/*AVR Interface, Port 2*/
#define N808_CSR_2       0x50
#define N808_ADDR_2      0x51
#define N808_DAT_2       0x52
#define N808_RLY_CTL     0x54
#define N808_FBR_CTL     0x56

/*CSR[X] Register bits*/
#define N808_CSR_OP_REQ  (1 << 0)
#define N808_CSR_DIR_IN  (0 << 1)
#define N808_CSR_DIR_OUT (1 << 1)
#define N808_CSR_ERR     (1 << 3)
#define N808_CSR_WDI_A   (1 << 4)
#define N808_CSR_WDI_B   (1 << 5)

/******************************************************************************/
/* AVR Definitions                                                            */
/*----------------------------------------------------------------------------*/
/* "AVR Firmware, Dual Ported", Rev 0.4, Aug 01, 2014                         */
/******************************************************************************/
#define N808_AVR_PRODUCT_ID_L          0x02
#define N808_AVR_PRODUCT_ID_H          0x03
#define N808_AVR_PRODUCT_REV           0x04

#define N808_AVR_HEART_BEAT_TIMEOUT_A  0x40
#define N808_AVR_COPPER_DEFAULT_MD_A   0x41
#define N808_AVR_FIBER_DEFAULT_MD_A    0x42
#define N808_AVR_PWR_OFF_MD_A          0x43
#define N808_AVR_RLY_STATUS_A          0x46
#define N808_AVR_CURRENT_MODE_A        0x47

#define N808_AVR_HEART_BEAT_TIMEOUT_B  0x48
#define N808_AVR_COPPER_DEFAULT_MD_B   0x49
#define N808_AVR_FIBER_DEFAULT_MD_B    0x4A
#define N808_AVR_PWR_OFF_MD_B          0x4B
#define N808_AVR_RLY_STATUS_B          0x4E
#define N808_AVR_CURRENT_MODE_B        0x4F

#define N808_AVR_HEART_BEAT_TIMEOUT_C  0x50
#define N808_AVR_COPPER_DEFAULT_MD_C   0x51
#define N808_AVR_FIBER_DEFAULT_MD_C    0x52
#define N808_AVR_PWR_OFF_MD_C          0x53
#define N808_AVR_RLY_STATUS_C          0x56
#define N808_AVR_CURRENT_MODE_C        0x57

#define N808_AVR_HEART_BEAT_TIMEOUT_D  0x58
#define N808_AVR_COPPER_DEFAULT_MD_D   0x59
#define N808_AVR_FIBER_DEFAULT_MD_D    0x5A
#define N808_AVR_PWR_OFF_MD_D          0x5B
#define N808_AVR_RLY_STATUS_D          0x5E
#define N808_AVR_CURRENT_MODE_D        0x5F

/*AVR Ports*/
enum {
   N808_AVR_PORT_1,
   N808_AVR_PORT_2,
};

static uint8_t avraddr[] = {N808_ADDR_1, N808_ADDR_2};
static uint8_t avrcsr[] =  {N808_CSR_1,  N808_CSR_2};
static uint8_t avrdata[] = {N808_DAT_1,  N808_DAT_2};

static uint8_t segwdi[] = {N808_CSR_WDI_A | N808_CSR_WDI_B, N808_CSR_WDI_A, N808_CSR_WDI_B};

/******************************************************************************/
/* CPLD access Functions                                                      */
/******************************************************************************/
static inline
uint8_t n808_cpld_read(uint8_t reg) {
    return (uint8_t)cvmx_mdio_read(N808_MDIO_BUS, N808_PHY, reg);
}

static inline
void n808_cpld_write(uint8_t reg, uint8_t data) {
    cvmx_mdio_write(N808_MDIO_BUS, N808_PHY, reg, data);
}

/******************************************************************************/
/* AVR access Functions                                                        */
/******************************************************************************/
static
uint8_t n808_avr_read(int port, uint8_t reg)
{
    n808_cpld_write(avraddr[port], reg);
    n808_cpld_write(avrcsr[port], N808_CSR_OP_REQ | N808_CSR_DIR_IN);

    while (n808_cpld_read(avrcsr[port]) & N808_CSR_OP_REQ)
        usleep(100);

    return (uint8_t)n808_cpld_read(avrdata[port]);
}

static
void n808_avr_write(int port, uint8_t reg, uint8_t data)
{
    n808_cpld_write(avraddr[port], reg);
    n808_cpld_write(avrdata[port], data);
    n808_cpld_write(avrcsr[port], N808_CSR_OP_REQ | N808_CSR_DIR_OUT);

    while (n808_cpld_read(avrcsr[port]) & N808_CSR_OP_REQ)
        usleep(100);
}

/******************************************************************************/
/* Niagara Interface Functions                                                */
/******************************************************************************/
int n808_avr_kick_heartbeat(int seg)
{
    n808_cpld_write(avrcsr[0], segwdi[seg]); //TODO: Port 1 or Port 2
    return 0;
}

int n808_timeout_get(int seg)
{
#if 0
    static uint8_t seg_regs[] = {0x0, 0x1};
    uint8_t m = n808_avr_read(seg_regs[seg - 1]);
    printf("Heartbeat timeout [%d]: %d msec\n", seg, (int) m * 100);
#endif
    return 0;
}

int n808_timeout_set(int seg, char *v)
{
#if 0
    static uint8_t seg_regs[] = {0x0, 0x1};
    int i = atoi(v); i /= 100;
    if (i <= 0xff) {
        n808_avr_write(seg_regs[seg - 1], (uint8_t)i);
        return 0;
    }
#endif
    return -1;
}

int n808_startup_wait_get(int seg)
{
#if 0
    uint8_t m = n808_avr_read(0x4);
    printf("Start up wait: %d sec\n", (int) m * 2);
#endif
    return 0;
}

int n808_startup_wait_set(int seg, char *v)
{
#if 0
    int i = atoi(v); i /= 2;
    if (i <= 0xff) {
        n808_avr_write(0x4, (uint8_t)i);
        return 0;
    }
#endif
    return -1;
}

int n808_startup_wait_ovr_get(int seg)
{
#if 0
    uint8_t m = n808_avr_read(0x5);
    printf("Start up wait override: %d\n", m);
#endif
    return 0;
}

int n808_startup_wait_ovr_set(int seg, char *v)
{
#if 0
    uint8_t i = atoi(v);
    if (i < 2) {
        n808_avr_write(0x5, i);
        return 0;
    }
#endif
    return -1;
}

int n808_default_mode_get(int seg)
{
#if 0
    static uint8_t seg_regs[] = {0x32, 0x38};
    uint8_t m = n808_avr_read(seg_regs[seg - 1]);
    printf("Default mode [%d]: %d\n", seg, (int) m);
#endif
    return 0;
}

int n808_default_mode_set(int seg, char *v)
{
#if 0
    static uint8_t seg_regs[] = {0x32, 0x38};
    uint8_t i = atoi(v);
    if (i < 6) {
        n808_avr_write(seg_regs[seg - 1], i);
        return 0;
    }
#endif
    return -1;
}

int n808_poweroff_mode_get(int seg)
{
#if 0
    static uint8_t seg_regs[] = {0x33, 0x39};
    uint8_t m = n808_avr_read(seg_regs[seg - 1]);
    printf("Power Off mode [%d]: %s\n", seg, (m)?"Bypass":"No Link");
#endif
    return 0;
}

int n808_poweroff_mode_set(int seg, char *v)
{
#if 0
    static uint8_t seg_regs[] = {0x33, 0x39};
    uint8_t i = atoi(v);
    if (i < 2) {
        n808_avr_write(seg_regs[seg - 1], i);
        return 0;
    }
#endif
    return -1;
}

int n808_current_mode_get(int seg)
{
#if 0
    static uint8_t seg_regs[] = {0x31, 0x37};
    uint8_t m = n808_avr_read(seg_regs[seg - 1]);
    printf("Current mode [%d]: %d\n", seg, (int) m);
#endif
    return 0;
}

int n808_current_mode_set(int seg, char *v)
{
    static uint8_t seg_regs[] = {N808_AVR_CURRENT_MODE_A, N808_AVR_CURRENT_MODE_B};
    uint8_t i = atoi(v);

    if (i < 6) {
        n808_avr_write(N808_AVR_PORT_1, seg_regs[seg - 1], i); //TODO: Port 1 or 2?
        return 0;
    }
    return -1;
}

int n808_status_get(int seg)
{
#if 0
    static uint8_t seg_regs[] = {0x2f, 0x35};
    static char *status[] = {"BYPASS", "INLINE", "NO LINK"};

    uint8_t s = n808_avr_read(seg_regs[seg - 1]);
    printf("Status [%d]: %s\n", seg, status[s - 1]);
#endif
    return 0;
}

int n808_oem_id_get(int seg)
{
#if 0
    uint8_t s = n808_avr_read(0x41);
    printf("OEM Id: %d\n", s);
#endif
    return 0;
}

int n808_product_rev_get(int seg)
{
#if 0
    uint8_t s = n808_avr_read(0x43);
    printf("Product revision: %d\n", s);
#endif
    return 0;
}

int n808_product_id_get(int seg)
{
#if 0
    uint8_t s = n808_avr_read(0x42);
    printf("Product Id: %d\n", s);
#endif
    return 0;
}

int n808_fw_rev_get(int seg)
{
#if 0
    uint8_t s = n808_avr_read(0x44);
    uint8_t ss = n808_avr_read(0x45);
    printf("Firmware revision: %d.%d\n", s, ss);
#endif
    return 0;
}

int n808_numseg_get(void)
{
    return 0; //TODO
}

int n808_probe(void)
/* Returns: IMTHW_N808 or IMTHW_UNDEF.
 * The detection is peformed based on the Product ID AVR register.
 * Note, the save value can be found in CPLD ID register (address = 0).
 */
{
    /*Enable access to the MDIO bus.*/
    cvmx_write_csr(CVMX_SMIX_EN(N808_MDIO_BUS), 0x1);

    /*Retrieve the Product ID value.*/
    uint8_t lo = n808_avr_read(N808_AVR_PORT_1, N808_AVR_PRODUCT_ID_L); //TODO: Port1 or 2?
    uint8_t hi = n808_avr_read(N808_AVR_PORT_1, N808_AVR_PRODUCT_ID_H);
    printf("Product Id: %x_%x\n", hi, lo); //TODO:debug
   //TODO:
   // 1. What value should I expect?
   // 2. 8- or 16-bits?

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

