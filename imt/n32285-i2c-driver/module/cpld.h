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

/* I2C CSR bits */
#define I2C_CSR_OP_1		0x20
#define I2C_CSR_OP_0		0x10
#define I2C_CSR_ERR		0x02
#define I2C_CSR_ST		0x01

#define I2C_CSR_READ		0x00
#define I2C_CSR_WRITE		(I2C_CSR_OP_0)
#define I2C_CSR_ACK_POLL	(I2C_CSR_OP_1)
#define I2C_STAT_MASK		0x07

/* I2C CTL bits */
#define I2C_CTL_A_WIDTH		0x40
#define I2C_CTL_RATE_1		0x20
#define I2C_CTL_RATE_0		0x10
#define I2C_CTL_SDA_O		0x08
#define I2C_CTL_SDL_O		0x04
#define I2C_CTL_SDA_I		0x02
#define I2C_CTL_SDL_I		0x01

#define I2C_CTL_RATE_1000	0x00
#define I2C_CTL_RATE_500	(I2C_CTL_RATE_0)
#define I2C_CTL_RATE_100	(I2C_CTL_RATE_1)
#define I2C_CTL_RATE_50		(I2C_CTL_RATE_1 | I2C_CTL_RATE_0)
#define I2C_CTL_SDA_SCL_HIGH	(I2C_CTL_SDA_O | I2C_CTL_SDL_O)

/* I2C STAT bits */
#define I2C_STAT_I2C_FLT	0x04
#define I2C_STAT_TOUT		0x02
#define I2C_STAT_NO_ACK		0x01

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

