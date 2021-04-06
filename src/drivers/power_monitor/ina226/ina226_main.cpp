
#include <px4_platform_common/getopt.h>
#include <px4_platform_common/module.h>

#include "ina226.h"

void
INA226::print_usage()
{
	PRINT_MODULE_DESCRIPTION(
		R"DESCR_STR(
### Description
Driver for the INA226 power monitor.

Multiple instances of this driver can run simultaneously, if each instance has a separate bus OR I2C address.

For example, one instance can run on Bus 2, address 0x41, and one can run on Bus 2, address 0x43.

If the INA226 module is not powered, then by default, initialization of the driver will fail. To change this, use
the -f flag. If this flag is set, then if initialization fails, the driver will keep trying to initialize again
every 0.5 seconds. With this flag set, you can plug in a battery after the driver starts, and it will work. Without
this flag set, the battery must be plugged in before starting the driver.

)DESCR_STR");

	PRINT_MODULE_USAGE_NAME("ina226", "driver");

	PRINT_MODULE_USAGE_COMMAND("start");
	PRINT_MODULE_USAGE_PARAMS_I2C_SPI_DRIVER(true, false);
	PRINT_MODULE_USAGE_PARAMS_I2C_ADDRESS(0x41);
	PRINT_MODULE_USAGE_PARAMS_I2C_KEEP_RUNNING_FLAG();
	PRINT_MODULE_USAGE_PARAM_INT('t', 1, 1, 2, "battery index for calibration values (1 or 2)", true);
	PRINT_MODULE_USAGE_DEFAULT_COMMANDS();
}

extern "C" int
ina226_main(int argc, char *argv[])
{
	int ch;
	using ThisDriver = INA226;
	BusCLIArguments cli{true, false};
	cli.i2c_address = INA226_BASEADDR;
	cli.default_i2c_frequency = 100000;
	cli.support_keep_running = true;
	cli.custom2 = 1;

	while ((ch = cli.getopt(argc, argv, "t:")) != EOF) {
		switch (ch) {
		case 't': // battery index
			cli.custom2 = (int)strtol(cli.optarg(), NULL, 0);
			break;
		}
	}

	const char *verb = cli.optarg();
	if (!verb) {
		ThisDriver::print_usage();
		return -1;
	}

	BusInstanceIterator iterator(MODULE_NAME, cli, DRV_POWER_DEVTYPE_INA226);

	if (!strcmp(verb, "start")) {
		return ThisDriver::module_start(cli, iterator);
	}

	if (!strcmp(verb, "stop")) {
		return ThisDriver::module_stop(iterator);
	}

	if (!strcmp(verb, "status")) {
		return ThisDriver::module_status(iterator);
	}

	ThisDriver::print_usage();
	return -1;
}
