#include <linux/acpi.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>

#include "int3472.h"

static int int3472_handle_gpio_resources(struct acpi_resource *ares, void *data)
{
	struct int3472_device *int3472 = data;
	union acpi_object *obj;
	int ret;

	if ((ares->type != ACPI_RESOURCE_TYPE_GPIO) ||
	   (ares->data.gpio.connection_type != ACPI_RESOURCE_GPIO_TYPE_IO))
		return EINVAL; /* Deliberately positive */

	/*
	 * gpio_n + 2 because the index of this _DSM function is 1-based and the
	 * first function is just a count.
	 */
	obj = acpi_evaluate_dsm_typed(int3472->adev->handle,
				      &cio2_pmic_gpio_guid, 0x00,
				      sensor->pmic.n_gpios + 2,
				      NULL, ACPI_TYPE_INTEGER);

        pr_info("Found GPIO with DSM data 0x%04x\n", obj->integer.value);

	if (!obj) {
		dev_warn(&sensor->adev->dev, "No _DSM entry for this GPIO pin\n");
		return -ENODEV;
	}

	switch (obj->integer.value & 0xff) { /* low byte holds type information */
	case 0x00: /* Purpose unclear, possibly a reset GPIO pin */
		ret = cio2_bridge_map_gpio_to_sensor(sensor, ares, "reset");
		break;
	case 0x01: /* Power regulators (we think) */
	case 0x0c:
		ret = cio2_bridge_register_regulator(sensor, ares);
		break;
	case 0x0b: /* Power regulators, but to a device separate to sensor */
		ret = cio2_bridge_register_regulator(sensor, ares);
		break;
	case 0x0d: /* Indicator LEDs */
		ret = cio2_bridge_map_gpio_to_sensor(sensor, ares, "indicator-led");
		break;
	default:
		/* if we've gotten here, we're clueless */
		dev_warn(&sensor->pmic.adev->dev,
			"GPIO type 0x%02llu not known; the sensor may not function\n",
			(obj->integer.value & 0xff));
		ret = EINVAL;
	}

	sensor->pmic.n_gpios++;

	ACPI_FREE(obj);
	return ret;
}

static int int3472_parse_crs(struct int3472_device *int3472)
{
	struct list_head resource_list;
	int ret = 0;

	INIT_LIST_HEAD(&resource_list);

	ret = acpi_dev_get_resources(int3472->adev, &resource_list,
				     int3472_handle_gpio_resources, int3472);

	acpi_dev_free_resource_list(&resource_list);

	return ret;
}

static int int3472_add(struct acpi_device *adev)
{
        struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };
        struct int3472_device *int3472;
        struct int3472_cldb cldb;
        union acpi_object *obj;
	acpi_status status;
	int ret = 0;

        /*
         * This driver is only intended to support "dummy" INT3472 devices
         * which appear in ACPI designed for Windows. These are distinguished
         * through the presence of a CLDB buffer with a particular value set.
         * We must be careful not to bind to INT3472 entries representing a
         * legitimate tps68470 device.
         */
	status = acpi_evaluate_object(adev->handle, "CLDB", NULL, &buffer);
	if (ACPI_FAILURE(status))
		return -ENODEV;

	obj = buffer.pointer;
	if (!obj) {
		dev_err(&adev->dev, "ACPI device has no CLDB object\n");
		return -ENODEV;
	}

	if (obj->type != ACPI_TYPE_BUFFER) {
		dev_err(&adev->dev, "CLDB object is not an ACPI buffer\n");
		ret = -EINVAL;
		goto out_free_buff;
	}

	if (obj->buffer.length > sizeof(cldb)) {
		dev_err(&adev->dev, "The CLDB buffer is too large\n");
		ret = -EINVAL;
		goto out_free_buff;
	}

        memcpy(&cldb, obj->buffer.pointer, obj->buffer.length);

        /*
         * control_logic_type = 1 indicates this does *not* represent a
         * phyiscal tps68470 PMIC. Any other value and we shouldn't try
         * to handle it
         */
        if (cldb.control_logic_type != 1) {
                ret = -EINVAL;
                goto out_free_buff;
        }

        int3472 = kzalloc(GFP_KERNEL, sizeof(*int3472));
        if (!int3472) {
                ret = -ENOMEM;
                goto out_free_buff;
        }

        int3472->adev = adev;
        adev->driver_data = int3472;

        int3472->sensor = acpi_dev_get_next_dep_dev(adev, NULL);

        ret = int3472_parse_crs(int3472);
        if (ret)
                goto out_free_buff;


        

out_free_buff:
        kfree(buffer.pointer);
        return 0;
}

static int int3472_remove(struct acpi_device *adev)
{
        return 0;
}

static const struct acpi_device_id int3472_device_id[] = {
        { "INT3472", 0 },
        { " ", 0},
};
MODULE_DEVICE_TABLE(acpi, int3472_device_id);

static struct acpi_driver int3472_driver = {
        .name = "INT3472 Discrete PMIC Driver",
        .ids = int3472_device_id,
        .ops = {
                .add = int3472_add,
                .remove = int3472_remove,
        },
        .owner = THIS_MODULE,
};

module_acpi_driver(int3472_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Dan Scally <djrscally@gmail.com>");
MODULE_DESCRIPTION("ACPI Driver for Discrete type INT3472 ACPI Devices");