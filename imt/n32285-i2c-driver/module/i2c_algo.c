#include "niagara_module.h"
#include "cpld.h"

static
int cpld_smbus_xfer(struct i2c_adapter *adap, uint16_t addr, uint16_t flags,
			char rw, uint8_t cmd, int size, union i2c_smbus_data *data)
{
	uint8_t rc = 0;
	uint8_t op = 0;

	if (flags & ~I2C_M_RD)
		return -EINVAL;

	cpld_write(I2C_DA, (addr & 0x7f) << 1);
	cpld_write(I2C_CTL, I2C_CTL_RATE_1000 | I2C_CTL_SDA_SCL_HIGH);

	switch (size) {
	case I2C_SMBUS_QUICK:
		op = I2C_CSR_ACK_POLL;
		break;
	case I2C_SMBUS_BYTE_DATA:
		cpld_write(I2C_ADDR_0, cmd);
		op = (rw == I2C_SMBUS_WRITE) ? I2C_CSR_WRITE : I2C_CSR_READ;
		break;
	}
	if (size != I2C_SMBUS_QUICK && rw == I2C_SMBUS_WRITE)
		cpld_write(I2C_DATA, data->byte);

	cpld_write(I2C_CSR, I2C_CSR_ST | op);

	do {
		udelay(100);
	} while (cpld_read(I2C_CSR) & I2C_CSR_ST);

	rc = cpld_read(I2C_STAT) & I2C_STAT_MASK;
	if (rc != 0)
		return -EIO;

	if (size != I2C_SMBUS_QUICK && rw == I2C_SMBUS_READ)
		data->byte = cpld_read(I2C_DATA);

	return 0;
}

static
uint32_t cpld_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_QUICK | I2C_FUNC_SMBUS_BYTE;
}

const struct i2c_algorithm i2c_cpld_algo = {
	.master_xfer = NULL,
	.smbus_xfer = cpld_smbus_xfer,
	.functionality = cpld_func,
};

