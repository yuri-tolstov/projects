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
#define N808_CPLD_ADDR 0x10

/*Bypass segments.*/
#define N808_NUMSEGS 2

/*Board signature (contained in CPLD_SIG_x registers)*/
#define N808_SIGN_L  0xAE
#define N808_SIGN_H  0x00

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
/*Misc Parameters*/
#define N808_AVR_FIRMWARE_REV          0x00
#define N808_AVR_SEC_FIRMWARE_REV      0x01
#define N808_AVR_PRODUCT_ID_L          0x02
#define N808_AVR_PRODUCT_ID_H          0x03
#define N808_AVR_PRODUCT_REV           0x04
#define N808_AVR_PRGM_TEST             0x13
#define N808_AVR_TEST_MODE             0x14
#define N808_AVR_FIBER_EN              0x15
#define N808_AVR_CURRENT_FIBER_EN      0x16
#define N808_AVR_CURRENT_F_TEMP        0x17
#define N808_AVR_PASSWORD_ENTER        0x17
#define N808_AVR_ERR_STAT_1            0x20
#define N808_AVR_ERR_STAT_2            0x21

/*AVR EEPROM Based Parameters*/
#define N808_AVR_ST_UP_DLY             0x23
#define N808_AVR_ST_UP_DLY_OVR         0x24
#define N808_AVR_MAX_TM_ERR            0x25
#define N808_AVR_I2C_CTL               0x29
#define N808_AVR_TEMP_CTL              0x2A
#define N808_AVR_TEMP_SDWN_CNT         0x2B
#define N808_AVR_ALERT_LIMIT           0x2C
#define N808_AVR_OVERT_LIMIT           0x2D
#define N808_AVR_PSSWD_E2PROM_BADDR    0x30 

/*Channel Specific Parameters*/
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
    return (uint8_t)cvmx_mdio_read(N808_MDIO_BUS, N808_CPLD_ADDR, reg);
}

static inline
void n808_cpld_write(uint8_t reg, uint8_t data) {
    cvmx_mdio_write(N808_MDIO_BUS, N808_CPLD_ADDR, reg, data);
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
    n808_cpld_write(avrcsr[N808_AVR_PORT_1], segwdi[seg]);
    return 0;
}

int n808_timeout_get(int seg)
{
    static uint8_t regs[] = {N808_AVR_HEART_BEAT_TIMEOUT_A, N808_AVR_HEART_BEAT_TIMEOUT_B};
    uint8_t m = n808_avr_read(N808_AVR_PORT_1, regs[seg - 1]);
    printf("Heartbeat timeout [%d]: %d msec\n", seg, (int) m * 100);
    return 0;
}

int n808_timeout_set(int seg, char *v)
{
    static uint8_t regs[] = {N808_AVR_HEART_BEAT_TIMEOUT_A, N808_AVR_HEART_BEAT_TIMEOUT_B};
    int i = atoi(v); i /= 100;
    if (i <= 0xff) {
        n808_avr_write(N808_AVR_PORT_1, regs[seg - 1], (uint8_t)i);
        return 0;
    }
    return -1;
}

int n808_startup_wait_get(int seg)
{
    uint8_t m = n808_avr_read(N808_AVR_PORT_1, N808_AVR_ST_UP_DLY);
    printf("Start up wait: %d sec\n", (int) m * 2);
    return 0;
}

int n808_startup_wait_set(int seg, char *v)
{
    int i = atoi(v); i /= 2;
    if (i <= 0xff) {
        n808_avr_write(N808_AVR_PORT_1, N808_AVR_ST_UP_DLY, (uint8_t)i);
        return 0;
    }
    return -1;
}

int n808_startup_wait_ovr_get(int seg)
{
    uint8_t m = n808_avr_read(N808_AVR_PORT_1, N808_AVR_ST_UP_DLY_OVR);
    printf("Start up wait override: %d\n", m);
    return 0;
}

int n808_startup_wait_ovr_set(int seg, char *v)
{
    uint8_t i = atoi(v);
    if (i < 2) {
        n808_avr_write(N808_AVR_PORT_1, N808_AVR_ST_UP_DLY_OVR, (uint8_t)i);
        return 0;
    }
    return -1;
}

int n808_default_mode_get(int seg)
{
    static uint8_t regs[] = {N808_AVR_COPPER_DEFAULT_MD_A, N808_AVR_COPPER_DEFAULT_MD_B};
    uint8_t m = n808_avr_read(N808_AVR_PORT_1, regs[seg - 1]);
    printf("Default mode [%d]: %d\n", seg, (int) m);
    return 0;
}

int n808_default_mode_set(int seg, char *v)
{
    static uint8_t regs[] = {N808_AVR_COPPER_DEFAULT_MD_A, N808_AVR_COPPER_DEFAULT_MD_B};
    uint8_t i = atoi(v);
    if (i < 6) {
        n808_avr_write(N808_AVR_PORT_1, regs[seg - 1], i);
        return 0;
    }
    return -1;
}

int n808_poweroff_mode_get(int seg)
{
    static uint8_t regs[] = {N808_AVR_PWR_OFF_MD_A, N808_AVR_PWR_OFF_MD_B};
    uint8_t m = n808_avr_read(N808_AVR_PORT_1, regs[seg - 1]);
    printf("Power Off mode [%d]: %s\n", seg, (m)?"Bypass":"No Link");
    return 0;
}

int n808_poweroff_mode_set(int seg, char *v)
{
    static uint8_t regs[] = {N808_AVR_PWR_OFF_MD_A, N808_AVR_PWR_OFF_MD_B};
    uint8_t i = atoi(v);
    if (i < 2) {
        n808_avr_write(N808_AVR_PORT_1, regs[seg - 1], i);
        return 0;
    }
    return -1;
}

int n808_current_mode_get(int seg)
{
    static uint8_t regs[] = {N808_AVR_CURRENT_MODE_A, N808_AVR_CURRENT_MODE_B};
    uint8_t m = n808_avr_read(N808_AVR_PORT_1, regs[seg - 1]);
    printf("Current mode [%d]: %d\n", seg, (int)m);
    return 0;
}

int n808_current_mode_set(int seg, char *v)
{
    static uint8_t regs[] = {N808_AVR_CURRENT_MODE_A, N808_AVR_CURRENT_MODE_B};
    uint8_t i = atoi(v);

    if (i < 6) {
        n808_avr_write(N808_AVR_PORT_1, regs[seg - 1], i);
        return 0;
    }
    return -1;
}

int n808_status_get(int seg)
{
    static uint8_t regs[] = {N808_AVR_RLY_STATUS_A, N808_AVR_RLY_STATUS_B};
    static char *status[] = {"BYPASS", "INLINE", "NO LINK"};

    uint8_t v = n808_avr_read(N808_AVR_PORT_1, regs[seg - 1]); //Port 1 or 2?
    printf("Status [%d]: %s\n", seg, status[v - 1]);
    return 0;
}

int n808_oem_id_get(int seg)
{
    /*N808 doesn't have such register.*/
    return 0;
}

int n808_product_rev_get(int seg)
{
    uint8_t v = n808_avr_read(N808_AVR_PORT_1, N808_AVR_PRODUCT_REV);
    printf("Product revision: %d\n", v);
    return 0;
}

int n808_product_id_get(int seg)
{
    uint8_t lo = n808_avr_read(N808_AVR_PORT_1, N808_AVR_PRODUCT_ID_L);
    uint8_t hi = n808_avr_read(N808_AVR_PORT_1, N808_AVR_PRODUCT_ID_H);
    int id = hi << 8 | lo;
    printf("Product ID: %x\n", id);
    return id;
}

int n808_fw_rev_get(int seg)
{
    uint8_t v = n808_avr_read(N808_AVR_PORT_1, N808_AVR_FIRMWARE_REV);
    uint8_t vv = n808_avr_read(N808_AVR_PORT_1, N808_AVR_SEC_FIRMWARE_REV);
    printf("Firmware revision: %d.%d\n", v, vv);
    return 0;
}

int n808_numseg_get(void)
{
    return N808_NUMSEGS;
}

int n808_probe(void)
/* Returns: IMTHW_N808 or IMTHW_UNDEF.
 * The detection is peformed based on the Product ID AVR register.
 * Note, the save value can be found in CPLD ID register (address = 0).
 */
{
    /*Enable access to the MDIO bus.*/
    cvmx_write_csr(CVMX_SMIX_EN(N808_MDIO_BUS), 0x1);

    /*Retrieve and analyze the Product ID.*/
    uint8_t id = n808_cpld_read(N808_CPLD_SIG_L);
    if (id != N808_SIGN_L) {
        return IMTHW_UNDEF;
    }
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

