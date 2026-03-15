# Fundamental concepts

For SPIFFS, in ``make menuconfig``, choose ``Custom partition table CSV`` then name the partition table CSV file, like ``partitions.csv``

For SPIFFS partition, set up ``partitions.csv`` like this

```csv
# Name,		Type,	SubType,	Offset,	Size,	Flags
# Note: if you change the phy_init or app partition offset, make sure to change the offset in Kconfig.projbuild
nvs,		data,	nvs,		0x9000,	0x6000,
phy_init,	data,	phy,		0xf000,	0x1000,
factory,	app,	factory,	0x10000,0xF0000,
storage,	data,	spiffs,		,		448K,
```

For big size factory and storage, like this:
```csv
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 1M,
storage,  data, spiffs,  0x180000,1M, // Add the address to the storage parition for proper flashing
```
This partitions will take 2.5 MB of flash while the default flash size is 2MB.

You need to change the size with ``make menuconfig`` -> ``Serial flashser config`` -> ``Flash size``

## Note

* If factory size is small lile ``512K``, some big size program can't be booted.

* ``base_path`` in SPIFFS must have the value like ``/data``, it can't be ``/``.

# Uploading file

To upload file from computer to ESP32, there are 2 ways:

* Use ``spiffsgen.py`` program from ESP-IDF
* Use tool ``mkspiffs``

## spiffsgen.py

Copy file ``spiffsgen.py`` from ``esp-idf/components/spiffs``

Create a binary file (e.g ``data_output``) to flash to ESP32 from all files inside a folder (e.g ``data``):

```sh
python3 spiffsgen.py <image_size> <base_dir> <output_file>
```

E.g:

```sh
python3 spiffsgen.py 1048576 data data_output
```

Then flash ``data_output`` to address ``0x180000`` in the storage partition: ``esptool.py -p /dev/ttyUSB0 write_flash 0x180000 data_output``

Now all files from the ``data`` folder can be executed by ``spiffs`` function of ESP_IDF

## mkspiffs

``mkspiffs`` is a tool to build and unpack SPIFFS images.

Standard parameters when using ``mkspiffs``:

* Block Size: ``4096`` (standard for SPI Flash, in bytes, ``-b``)
* Page Size: ``256`` (standard for SPI Flash, in bytes, ``-p``)
* Image Size: Size of the partition in bytes (can be obtained from a partition table) (``-s``)
* Partition Offset: Starting address of the partition (can be obtained from a partition table)

**Step 1**: Clone ``mkspiffs`` from the [official Github](https://github.com/igrr/mkspiffs) 

**Step 2**: Go to that Github folder, run ``git submodule update --init``

**Step 3**: Then run ``build_all_configs.sh``. If running successfully, 4 folder will appear:

* ``mkspiffs-0.2.3-7-gf248296-arduino-esp32-linux64``
* ``mkspiffs-0.2.3-7-gf248296-arduino-esp8266-linux64``
* ``mkspiffs-0.2.3-7-gf248296-esp-idf-linux64``
* ``mkspiffs-0.2.3-7-gf248296-generic-linux64``

For ESP-IDF flashing on Linux 64-bit, choose ``mkspiffs-0.2.3-7-gf248296-esp-idf-linux64``. Inside ``mkspiffs-0.2.3-7-gf248296-esp-idf-linux64``, 
there is a file ``mkspiffs``.

All operation from now works inisde the ``mkspiffs-0.2.3-7-gf248296-esp-idf-linux64`` folder.

**Create binary file**

To create a ``data_output`` binary file from all files inside the ``data`` folder, it is known as packing file ``data`` into ``data_output``.

```
./mkspiffs -c data/ -b 4096 -p 256 -s 0x100000 data_output
```

Note that ``block * page = size``, e.g ``4096 * 256 = 1048576 = 0x100000``

To unpack ``data_output`` back to the normal file:

```
./mkspiffs -u output_folder data_output
```
**Flash binary file to SPIFFS partition**

To flash ``data_output`` to address ``0x180000`` in the storage partition: 

```
esptool.py -p /dev/ttyUSB0 write_flash 0x180000 data_output
```

Now all files from the ``data`` folder can be executed by ``spiffs`` function of ESP_IDF

**Read binary file from the SPIFFS partition**

Read the spiffs data from a specific address in the storage address location, use ``esptool``:

```
esptool.py -p /dev/ttyUSB0 read_flash <address_to_read> <size_of_data_to_read> <output_binary_file>
```

The result from that reading operation is a binary file. E.g:

```
esptool.py -p /dev/ttyUSB0 read_flash 0x180000 0x100000 output_binary_file
```

Then ``output_binary_file`` has to be unpacked back to normal file by ``mkspiffs``. Notice that the address to read and the size to read must 
be the address and size based on the partition size. If the address or size are wrong, the data can't be unpacked.

# Library

CPP library [esp_spiffs_cpp](src/esp_spiffs_cpp)

# Examples

* [esp32_spiffs_example.cpp](src/esp32_spiffs_example.cpp): Example use of ``esp_spiffs_cpp`` library

**Note**: The mount folder like ``/data`` is just the name of the mount partition for spiffs which doesn't effect the file executing operation.