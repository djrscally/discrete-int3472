struct int3472_cldb {
	u8 version;
	/*
	 * control logic type
	 * 0: UNKNOWN
	 * 1: DISCRETE(CRD-D)
	 * 2: PMIC TPS68470
	 * 3: PMIC uP6641
	 */
	u8 control_logic_type;
	u8 control_logic_id;
	u8 sensor_card_sku;
	u8 reserved[28];   
};

struct int3472_device {
        struct acpi_device *adev;
        struct acpi_device *sensor;
        struct gpiod_lookup_table gpios;
        struct list_head regulators;
        unsigned int n_gpios;
};