/* CPLD Register Address*/
#define AVR_ADDR                0x00
#define AVR_DAT                 0x01
#define HOST_DAT                0x02
#define STAT                    0x03
#define CTL_0                   0x04
#define CTL_1                   0x05
#define MAC_CTL                 0x06
#define CSR                     0x07
#define I2C_CSR                 0x08
#define I2C_DA                  0x09
#define I2C_DATA                0x0A
#define I2C_STAT                0x0B
#define I2C_ADDR_0              0x0C
#define I2C_ADDR_1              0x0D
#define I2C_CTL                 0x0E

/* CSR bits */
#define SW_RSTN                 0x01

#define NXX_EECD                0x00010      /* EEPROM/Flash Control - RW */
#define NXX_CTRL                0x00000      /* Device Control - RW */
#define NXX_CTRL_EXT            0x00018      /* Extended Device Control - RW */
#define NXX_MDI_CTL             0x00020      /* MDIO Control Reg */
#define NXX_LEDCTL              0x00200      /* LED Control */
#define NXX_MSCA                0x0425c      /* MDI Cmd/Addr */
#define NXX_MSRWD               0x04260      /* MDI Rd/Wr Data */
#define NXX_ESDP                0x00020      /* Ext. SDP Ctrl */
#define NXX_EODSDP              0x00028      /* Ext. OD SDP Ctrl */

#define NXX_LED_CTL             0x00e00      /* LED Control Reg*/
#define NXX_CTRL_EXT_SDP4_DIR   0x00000100   /* Direction of SDP4 0=in 1=out */
#define NXX_CTRL_EXT_SDP5_DIR   0x00000200   /* Direction of SDP5 0=in 1=out */
#define NXX_CTRL_EXT_SDP6_DIR   0x00000400   /* Direction of SDP6 0=in 1=out */
#define NXX_CTRL_EXT_SDP7_DIR   0x00000800   /* Direction of SDP7 0=in 1=out */

#define NXX_CTRL_EXT_SDP4_DATA  0x00000010   /* Value of SW Defineable Pin 4 */
#define NXX_CTRL_EXT_SDP5_DATA  0x00000020   /* Value of SW Defineable Pin 5 */
#define NXX_CTRL_EXT_SDP6_DATA  0x00000040   /* Value of SW Defineable Pin 6 */
#define NXX_CTRL_EXT_SDP7_DATA  0x00000080   /* Value of SW Defineable Pin 7 */

#define NXX_CTRL_SWDPIN0        0x00040000   /* SWDPIN 0 value */
#define NXX_CTRL_SWDPIN1        0x00080000   /* SWDPIN 1 value */
#define NXX_CTRL_SWDPIN2        0x00100000   /* SWDPIN 2 value */
#define NXX_CTRL_SWDPIN3        0x00200000   /* SWDPIN 3 value n22xx_hd_node_data */

#define NXX_CTRL_SWDPIO0        0x00400000   /* SWDPIN 0 Input or output */
#define NXX_CTRL_SWDPIO1        0x00800000   /* SWDPIN 1 input or output */
#define NXX_CTRL_SWDPIO2        0x01000000   /* SWDPIN 2 input or output */
#define NXX_CTRL_SWDPIO3        0x02000000   /* SWDPIN 3 input or output */

/* Port B */
#define IM_WDI_DIR              NXX_CTRL_EXT_SDP7_DIR
#define IM_SPI_CLK_DIR          NXX_CTRL_SWDPIO1
#define IM_SPI_MISO_DIR         NXX_CTRL_SWDPIO0
#define IM_SPI_MOSI_DIR         NXX_CTRL_EXT_SDP6_DIR
#define IM_WDI_DATA             NXX_CTRL_EXT_SDP7_DATA
#define IM_SPI_CLK_DATA         NXX_CTRL_SWDPIN1
#define IM_SPI_MISO_DATA        NXX_CTRL_SWDPIN0
#define IM_SPI_MOSI_DATA        NXX_CTRL_EXT_SDP6_DATA

/* Port A */
#define IM_RELAY_ON_DIR         NXX_CTRL_EXT_SDP7_DIR
#define IM_RELAY_ON_DATA        NXX_CTRL_EXT_SDP7_DATA
#define IM_SPI_RD_CMD           0x80

#define NXX_EECD_SK             0x00000001    /* EEPROM Clock */
#define NXX_EECD_CS             0x00000002    /* EEPROM Chip Select */
#define NXX_EECD_DI             0x00000004    /* EEPROM Data In */
#define NXX_EECD_DO             0x00000008    /* EEPROM Data Out */

/* EEPROM Read */
#define NXX_EERD                0x00014        /* EEPROM Read - RW */
#define NXX_EERD_START          0x00000001     /* Start Read */
#define NXX_EERD_DONE           0x00000010     /* Read Done */
#define NXX_EERD_ADDR_SHIFT     8
#define NXX_EERD_ADDR_MASK      0x0000FF00     /* Read Address */
#define NXX_EERD_DATA_SHIFT     16
#define NXX_EERD_DATA_MASK      0xFFFF0000     /* Read Data */

/* EEPROM WRITE */
#define NXX_EEWR                0x0102C        /* EEPROM Write Register - RW */

#define NXX_EEPROM_GRANT_ATTEMPTS  1000        /* EEPROM # attempts to gain grant */
#define NXX_EEPROM_POLL_READ    0              /* Flag for polling for read complete */
#define NXX_EEPROM_RW_REG_DONE  2              /* Offset to READ/WRITE done bit */
#define NXX_EEPROM_RW_REG_DATA  16             /* Offset to data in EEPROM read/write registers */
#define NXX_EECD_FWE_MASK       0x00000030
#define NXX_EECD_FWE_DIS        0x00000010     /* Disable FLASH writes */
#define NXX_EECD_FWE_EN         0x00000020     /* Enable FLASH writes */
#define NXX_EECD_REQ            0x00000040     /* EEPROM Access Request */
#define NXX_EECD_GNT            0x00000080     /* EEPROM Access Grant */
#define NXX_EECD_PRES           0x00000100     /* EEPROM Present */
#define NXX_EECD_TYPE           0x00002000     /* EEPROM Type (1-SPI, 0-Microwire) */

#define IM_NS_DIR               NXX_CTRL_SWDPIO0
#define IM_A0_DIR               NXX_CTRL_EXT_SDPn2264_extra_setup6_DIR
#define IM_A1_DIR               NXX_CTRL_EXT_SDP7_DIR

#define IM_NS_DATA              NXX_CTRL_SWDPIN0
#define IM_A0_DATA              NXX_CTRL_EXT_SDP6_DATA
#define IM_A1_DATA              NXX_CTRL_EXT_SDP7_DATA

/* Port B */
#define IM_A2_DIR               NXX_CTRL_SWDPIO0
#define IM_A3_DIR               NXX_CTRL_EXT_SDP6_DIR
#define IM_DIR_DIR              NXX_CTRL_EXT_SDP7_DIR

#define IM_A2_DATA              NXX_CTRL_SWDPIN0
#define IM_A3_DATA              NXX_CTRL_EXT_SDP6_DATA
#define IM_DIR_DATA             NXX_CTRL_EXT_SDP7_DATA

#define IM_D0_DIR               NXX_CTRL_SWDPIO0
#define IM_D1_DIR               NXX_CTRL_EXT_SDP6_DIR
#define IM_D2_DIR               NXX_CTRL_EXT_SDP7_DIR

#define IM_D0_DATA              NXX_CTRL_SWDPIN0
#define IM_D1_DATA              NXX_CTRL_EXT_SDP6_DATA
#define LFD_OPT_MSK             0x03

#define IM_D2_DATA              NXX_CTRL_EXT_SDP7_DATA

/* Port A */
#define IM_D3_DIR               NXX_CTRL_SWDPIO0
#define IM_D4_DIR               NXX_CTRL_EXT_SDP6_DIR
#define IM_DS_DIR               NXX_CTRL_EXT_SDP7_DIR
#define LFD_OPT_MSK             0x03

#define IM_D3_DATA              NXX_CTRL_SWDPIN0
#define IM_D4_DATA              NXX_CTRL_EXT_SDP6_DATA
#define IM_DS_DATA              NXX_CTRL_EXT_SDP7_DATA

/* Serial EEPROM op code */
#define WREN                    0x06
#define WRDI                    0x04
#define RDSR                    0x05
#define WRSR                    0x01
#define S_READ                  0x03
#define S_WRITE                 0x02

/* Definition for 82571 SMI interface */
#define PHY_REG_MSK             (0x1F << 16)
#define PHY_ADDR_MSK            (0x1 << 21)
#define PHY_RD_OP               (2 << 26)
#define PHY_WR_OP               (1 << 26)
#define PHY_RDY_MSK             (1 << 28)
#define MAX_PHY_RETRY           200

/* Defintion for PHY ctl register bit mask */
#define PHY_CTL_REG             0x0
#define PHY_PWR_DWN_MSK         (0x1 << 11)

#define PHY_STATUS              0x01
#ifndef MII_SR_LINK_STATUS
#define MII_SR_LINK_STATUS      (1 << 2)
#endif

